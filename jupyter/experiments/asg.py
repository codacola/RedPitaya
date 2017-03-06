import os
import fcntl
import mmap

import ctypes
import numpy as np
from enum import Enum

class asg (object):
    # sampling frequency
    FS = 125000000.0
    # linear addition multiplication register width
    DW  = 14
    DWM = 14
    DWS = 14
    # fixed point range
    DWr  = (1 << (DW -1)) - 1
    DWMr = (1 << (DWM-2))
    DWSr = (1 << (DWS-1)) - 1
    # buffer parameters
    CWM = 14  # counter width magnitude (fixed point integer)
    CWF = 16  # counter width fraction  (fixed point fraction)
    N = 2**CWM # table size
    # control register masks
    CTL_TRG_MASK = np.uint32(1<<3) # 1 - sw trigger bit (sw trigger must be enabled)
    CTL_STP_MASK = np.uint32(1<<2) # 1 - stop/abort; returns 1 when stopped
    CTL_STR_MASK = np.uint32(1<<1) # 1 - start
    CTL_RST_MASK = np.uint32(1<<0) # 1 - reset state machine so that it is in known state

    regset_dtype = np.dtype([
        # control/status
        ('ctl_sts', 'uint32'),
        ('rsv0'   , 'uint32', 3),  # reserved
        # start/stop/trigger masks
        ('cfg_str', 'uint32'),  # start
        ('cfg_stp', 'uint32'),  # stop
        ('cfg_trg', 'uint32'),  # trigger
        ('rsv1'   , 'uint32', 1),
        # buffer configuration
        ('cfg_siz', 'uint32'),  # size
        ('cfg_off', 'uint32'),  # offset
        ('cfg_stp', 'uint32'),  # step
        ('rsv2'   , 'uint32', 1),
        # burst mode
        ('cfg_bmd', 'uint32'),  # mode [1:0] = [inf, ben]
        ('cfg_bdl', 'uint32'),  # data length
        ('cfg_bln', 'uint32'),  # length (data+pause)
        ('cfg_bnm', 'uint32'),  # number of bursts pulses
        # burst status
        ('sts_bln', 'uint32'),  # length (current position inside burst length)
        ('sts_bnm', 'uint32'),  # number (current burst counter)
        # linear transformation
        ('cfg_mul',  'int32'),  # multiplier (amplitude)
        ('cfg_sum',  'int32')   # addedr (offset)
    ])

    def __init__ (self, index:int, uio:str = '/dev/uio/asg'):
        """Module instance index should be provided"""

        uio = uio+str(index)

        # open device file
        try:
            self.uio_dev = os.open(uio, os.O_RDWR | os.O_SYNC)
        except OSError as e:
            raise IOError(e.errno, "Opening {}: {}".format(uio, e.strerror))

        # exclusive lock
        try:
            fcntl.flock(self.uio_dev, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except IOError as e:
            raise IOError(e.errno, "Locking {}: {}".format(uio, e.strerror))

        # map regset
        try:
            self.uio_reg = mmap.mmap(
                fileno=self.uio_dev, length=mmap.PAGESIZE, offset=0x0,
                flags=mmap.MAP_SHARED, prot=(mmap.PROT_READ | mmap.PROT_WRITE))
        except OSError as e:
            raise IOError(e.errno, "Mapping (regset) {}: {}".format(uio, e.strerror))

        regset_array = np.recarray(1, self.regset_dtype, buf=self.uio_reg)
        self.regset = regset_array[0]

        # map buffer table
        try:
            self.uio_tbl = mmap.mmap(
                # TODO: probably the length should be rounded up to mmap.PAGESIZE
                fileno=self.uio_dev, length=4*self.N, offset=mmap.PAGESIZE,
                flags=mmap.MAP_SHARED, prot=(mmap.PROT_READ | mmap.PROT_WRITE))
        except OSError as e:
            raise IOError(e.errno, "Mapping (buffer) {}: {}".format(uio, e.strerror))

        #table_array = np.recarray(1, self.table_dtype, buf=self.uio_tbl)
        self.table = np.frombuffer(self.uio_tbl, 'int32')

    def __del__ (self):
        self.uio_tbl.close()
        self.uio_reg.close()
        try:
            os.close(self.uio_dev)
        except OSError as e:
            raise IOError(e.errno, "Closing {}: {}".format(uio, e.strerror))

    def show_regset (self):
        print (
            "ctl_sts = 0x{reg:x} = {reg:d}  # control/status                 \n".format(reg=self.regset.ctl_sts)+
            "cfg_str = 0x{reg:x} = {reg:d}  # mask start                     \n".format(reg=self.regset.cfg_str)+
            "cfg_stp = 0x{reg:x} = {reg:d}  # mask stop                      \n".format(reg=self.regset.cfg_stp)+
            "cfg_trg = 0x{reg:x} = {reg:d}  # mask trigger                   \n".format(reg=self.regset.cfg_trg)+
            "cfg_siz = 0x{reg:x} = {reg:d}  # table size                     \n".format(reg=self.regset.cfg_siz)+
            "cfg_off = 0x{reg:x} = {reg:d}  # table offset                   \n".format(reg=self.regset.cfg_off)+
            "cfg_stp = 0x{reg:x} = {reg:d}  # table step                     \n".format(reg=self.regset.cfg_stp)+
            "cfg_bmd = 0x{reg:x} = {reg:d}  # burst mode [1:0] = [inf, ben]  \n".format(reg=self.regset.cfg_bmd)+
            "cfg_bdl = 0x{reg:x} = {reg:d}  # burst data length              \n".format(reg=self.regset.cfg_bdl)+
            "cfg_bln = 0x{reg:x} = {reg:d}  # burst length (data+pause)      \n".format(reg=self.regset.cfg_bln)+
            "cfg_bnm = 0x{reg:x} = {reg:d}  # burst number of bursts pulses  \n".format(reg=self.regset.cfg_bnm)+
            "sts_bln = 0x{reg:x} = {reg:d}  # burst length (current position)\n".format(reg=self.regset.sts_bln)+
            "sts_bnm = 0x{reg:x} = {reg:d}  # burst number (current counter) \n".format(reg=self.regset.sts_bnm)+
            "cfg_mul = 0x{reg:x} = {reg:d}  # multiplier (amplitude)         \n".format(reg=self.regset.cfg_mul)+
            "cfg_sum = 0x{reg:x} = {reg:d}  # addedr (offset)                \n".format(reg=self.regset.cfg_sum)
        )

    def reset (self):
        # reset state machine
        self.regset.ctl_sts = self.CTL_RST_MASK

    def start (self):
        # reset state machine
        self.regset.ctl_sts = self.CTL_STR_MASK

    def stop (self):
        # reset state machine
        self.regset.ctl_sts = self.CTL_STP_MASK

    def trigger (self):
        # activate SW trigger
        self.regset.ctl_sts = self.CTL_TRG_MASK

    def status (self) -> tuple:
        # [start, trigger] status
        return (bool(self.regset.ctl_sts & self.CTL_STR_MASK),
                bool(self.regset.ctl_sts & self.CTL_TRG_MASK))

    @property
    def mask (self) -> tuple:
        """Enable masks for [start, stop, trigger] signals"""
        return ([self.regset.cfg_str,
                 self.regset.cfg_stp,
                 self.regset.cfg_trg])

    @mask.setter
    def mask (self, value: tuple):
        """Enable masks for [start, stop, trigger] signals"""
        self.regset.cfg_str = value [0]
        self.regset.cfg_stp = value [1]
        self.regset.cfg_trg = value [2]

    @property
    def amplitude (self) -> float:
        """Output amplitude in vols"""
        return (self.regset.cfg_mul / self.DWMr)

    @amplitude.setter
    def amplitude (self, value: float):
        """Output amplitude in vols"""
        # TODO: fix saturation
        if (-1.0 <= value <= 1.0):
            self.regset.cfg_mul = value * self.DWMr
        else:
            raise ValueError("Output amplitude should be inside [-1,1]")

    @property
    def offset (self) -> float:
        """Output offset in vols"""
        return (self.regset.cfg_sum / self.DWSr)

    @offset.setter
    def offset (self, value: float):
        """Output offset in vols"""
        # TODO: fix saturation
        if (-1.0 <= value <= 1.0):
            self.regset.cfg_sum = value * self.DWSr
        else:
            raise ValueError("Output offset should be inside [-1,1]")

    @property
    def frequency (self) -> float:
        """Frequency in Hz"""
        siz = self.regset.cfg_siz + 1
        stp = self.regset.cfg_stp + 1
        return (stp / siz * self.FS)

    @frequency.setter
    def frequency (self, value: float):
        """Frequency in Hz"""
        if (value < self.FS/2):
            siz = self.regset.cfg_siz + 1
            self.regset.cfg_stp = int(siz * (value / self.FS)) - 1
        else:
            raise ValueError("Frequency should be less then half the sample rate. f < FS/2 = {}".format(self.FS/2))

    @property
    def phase (self) -> float:
        """Phase in angular degrees"""
        siz = self.regset.cfg_siz + 1
        off = self.regset.cfg_off
        return (stp / siz * 360)

    @phase.setter
    def phase (self, value: float):
        """Phase in angular degrees"""
        # TODO add range check
        siz = self.regset.cfg_siz + 1
        self.regset.cfg_off = int(siz * (value % 360) / 360)

    @property
    def waveform (self):
        """Waveworm table containing normalized values in the range [-1,1]"""
        siz = (self.regset.cfg_siz + 1) >> self.CWF
        # TODO: nparray
        return [self.table[i] / self.DWr for i in range(siz)]

    @waveform.setter
    def waveform (self, value):
        """Waveworm table containing normalized values in the range [-1,1]"""
        # TODO check table size shape
        siz = len(value)
        if (siz <= self.N):
            for i in range(siz):
                # TODO add saturation
                self.table[i] = int(value[i] * self.DWr)
            self.regset.cfg_siz = (siz << self.CWF) - 1
        else:
            raise ValueError("Waveform table size should not excede buffer size. N = {}".format(self.N))

    class modes(Enum):
        CONTINUOUS = ctypes.c_uint32(0x0)
        BURST_FIN  = ctypes.c_uint32(0x1)
        BURST_INF  = ctypes.c_uint32(0x3)

    @property
    def mode (self):
        return (modes(self.regset.cfg_bst))

    @mode.setter
    def mode (self, value):
        # TODO check range
        self.regset.cfg_bst = value

    @property
    def burst_repetitions (self) -> int:
        return (self.regset.cfg_bnm)

    @burst_repetitions.setter
    def burst_repetitions (self, value: int):
        # TODO check range
        self.regset.cfg_bnm = value

    @property
    def burst_data_len (self) -> int:
        return (self.regset.cfg_bdl)

    @burst_data_len.setter
    def burst_data_len (self, value: int):
        # TODO check range
        self.regset.cfg_bdl = value

    @property
    def burst_period_len (self) -> int:
        return (self.regset.cfg_bln)

    @burst_period_len.setter
    def burst_period_len (self, value: int):
        # TODO check range
        self.regset.cfg_bln = value