/**
 * @file
 * @copyright  Copyright 2016 GNSS Sensor Ltd. All right reserved.
 * @author     Sergey Khabarov - sergeykhbr@gmail.com
 * @brief      SystemC CPU wrapper. To interact with the SoC simulator.
 */

#include "api_core.h"
#include "rtl_wrapper.h"

namespace debugger {

static const uint32_t TEST_ROM[] {
    0x00000093,//          	[1000] li	ra,0
    0x00000113,//          	[1004] li	sp,0
    0x00000193,//          	[1008] li	gp,0
    0x55000213,//          	[100C] li	tp,550
    0x00023083,//          	[1010] ld	ra,0(tp) # 0 <_tbss_end>
    0x00823403,//          	[1014] ld	s0,8(tp) # 8 <_tbss_end+0x8>
    0x01023483,//          	[1018] ld	s1,16(tp) # 10 <_tbss_end+0x10>
    0x77700213,//          	[101C] li	tp,777
    0x00000313,//          	[1020] li	t1,0
    0x00000393,//          	[1024] li	t2,0
    0x00000413,//          	[1028] li	s0,0
    0x00100113,//          	[102C] li	sp,1
    0x01311113,//          	[1030] slli	sp,sp,0x13
    0x00228133,//          	[1034] add	sp,t0,sp
    0x00008013,//          	[1038] nop
    0x00008013,//          	[103C] nop
    
    0x00100513,//          	[1040] 10b0:	li	a0,1
    0x00008013,//          	[1044] nop
    //0x00100593,//          	[1048] 10b0:	li	a1,1
    0x00000593,//          	[1048] 10b0:	li	a1,0
    0x00b57063,//          	[104C] 10b4:	bleu	a1,a0,10b4 <_start+0xb4>
    0,
};

RtlWrapper::RtlWrapper(sc_module_name name)
    : sc_module(name),
    o_clk("clk", 1, SC_NS) {

    clockCycles_ = 1000000; // 1 MHz when default resolution = 1 ps

    SC_METHOD(registers);
    sensitive << o_clk.posedge_event();

    SC_METHOD(comb);
    sensitive << r.nrst;
    sensitive << r.resp_mem_data_valid;
    sensitive << r.resp_mem_data;
    sensitive << r.interrupt;

    SC_METHOD(clk_negedge_proc);
    sensitive << o_clk.negedge_event();

    w_nrst = 0;
    v.nrst = 0;
    v.interrupt = false;
    v.resp_mem_data = 0;
    v.resp_mem_data_valid = false;
}

void RtlWrapper::clk_gen() {
    // todo: instead sc_clock
}

void RtlWrapper::comb() {
    o_nrst = r.nrst.read()[1];
    o_resp_mem_data_valid = r.resp_mem_data_valid;
    o_resp_mem_data = r.resp_mem_data;
    o_interrupt = r.interrupt;

    if (!r.nrst.read()[1]) {
    }
}

void RtlWrapper::registers() {
    v.nrst = (r.nrst.read() << 1) | w_nrst;

    r = v;
}

void RtlWrapper::clk_negedge_proc() {
    /** Simulation events queue */
    IFace *cb;
    queue_.initProc();
    queue_.pushPreQueued();
    uint64_t step_cnt = i_timer.read();
    while (cb = queue_.getNext(step_cnt)) {
        static_cast<IClockListener *>(cb)->stepCallback(step_cnt);
    }

    /** */
    v.resp_mem_data = 0;
    v.resp_mem_data_valid = false;
    if (i_req_mem_valid.read()) {
        uint64_t addr = i_req_mem_addr.read();
        Reg64Type val;
        if (i_req_mem_write.read()) {
            uint8_t strob = i_req_mem_strob.read();
            uint64_t offset = mask2offset(strob);
            int size = mask2size(strob >> offset);

            addr += offset;
            val.val = i_req_mem_data.read();
            ibus_->write(addr, val.buf, size);
            v.resp_mem_data = 0;
        } else {
#if 0
            int idx = ((addr - 0x1000) % sizeof(TEST_ROM)) / 4;
            v.resp_mem_data = *((uint64_t*)&TEST_ROM[idx]);
#else
            ibus_->read(addr, val.buf, sizeof(val));
            v.resp_mem_data = val.val;
#endif
        }
        v.resp_mem_data_valid = true;
    }
}

uint64_t RtlWrapper::mask2offset(uint8_t mask) {
    for (int i = 0; i < AXI_DATA_BYTES; i++) {
        if (mask & 0x1) {
            return static_cast<uint64_t>(i);
        }
        mask >>= 1;
    }
    return 0;
}

uint32_t RtlWrapper::mask2size(uint8_t mask) {
    uint32_t bytes = 0;
    for (int i = 0; i < AXI_DATA_BYTES; i++) {
        if (!(mask & 0x1)) {
            break;
        }
        bytes++;
        mask >>= 1;
    }
    return bytes;
}

void RtlWrapper::setClockHz(double hz) {
    sc_time dt = sc_get_time_resolution();
    clockCycles_ = static_cast<int>((1.0 / hz) / dt.to_seconds() + 0.5);
}
    
void RtlWrapper::registerStepCallback(IClockListener *cb, uint64_t t) {
    queue_.put(t, cb);
}

void RtlWrapper::raiseSignal(int idx) {
    switch (idx) {
    case CPU_SIGNAL_RESET:
        w_nrst = 1;
        break;
    case CPU_SIGNAL_EXT_IRQ:
        v.interrupt = true;
        break;
    default:;
    }
}

void RtlWrapper::lowerSignal(int idx) {
    switch (idx) {
    case CPU_SIGNAL_RESET:
        w_nrst = 0;
        break;
    case CPU_SIGNAL_EXT_IRQ:
        v.interrupt = false;
        break;
    default:;
    }
}



}  // namespace debugger
