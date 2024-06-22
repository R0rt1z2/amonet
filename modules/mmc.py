# based on mmc driver from amonet
# see https://github.com/xyzz/amonet

from enum import Enum, IntFlag
import time
from struct import pack, unpack


class Mmc:
    class Opcode(Enum):
        MMC_GO_IDLE_STATE = 0
        MMC_SEND_OP_COND = 1
        MMC_ALL_SEND_CID = 2
        MMC_SET_RELATIVE_ADDR = 3
        MMC_SET_DSR = 4
        MMC_SLEEP_AWAKE = 5
        MMC_SWITCH = 6
        MMC_SELECT_CARD = 7
        MMC_SEND_EXT_CSD = 8
        MMC_SEND_CSD = 9
        MMC_SEND_CID = 10
        MMC_READ_DAT_UNTIL_STOP = 11
        MMC_STOP_TRANSMISSION = 12
        MMC_SEND_STATUS = 13
        MMC_BUS_TEST_R = 14
        MMC_GO_INACTIVE_STATE = 15
        MMC_BUS_TEST_W = 19
        MMC_SPI_READ_OCR = 58
        MMC_SPI_CRC_ON_OFF = 59

        MMC_SET_BLOCKLEN = 16
        MMC_READ_SINGLE_BLOCK = 17
        MMC_READ_MULTIPLE_BLOCK = 18
        MMC_SEND_TUNING_BLOCK = 19
        MMC_SEND_TUNING_BLOCK_HS200 = 21

        MMC_WRITE_DAT_UNTIL_STOP = 20

        MMC_SET_BLOCK_COUNT = 23
        MMC_WRITE_BLOCK = 24
        MMC_WRITE_MULTIPLE_BLOCK = 25
        MMC_PROGRAM_CID = 26
        MMC_PROGRAM_CSD = 27

        MMC_SET_WRITE_PROT = 28
        MMC_CLR_WRITE_PROT = 29
        MMC_SEND_WRITE_PROT = 30

        MMC_ERASE_GROUP_START = 35
        MMC_ERASE_GROUP_END = 36
        MMC_ERASE = 38

        MMC_FAST_IO = 39
        MMC_GO_IRQ_STATE = 40

        MMC_LOCK_UNLOCK = 42

        MMC_APP_CMD = 55
        MMC_GEN_CMD = 56

        SD_SEND_IF_CONF = 8
        SD_APP_OP_COND = 41

    class Flags(IntFlag):
        MMC_RSP_PRESENT = 1 << 0
        MMC_RSP_136 = 1 << 1
        MMC_RSP_CRC = 1 << 2
        MMC_RSP_BUSY = 1 << 3
        MMC_RSP_OPCODE = 1 << 4

        MMC_RSP_NONE = 0
        MMC_RSP_R1 = MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE
        MMC_RSP_R1B = MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE | MMC_RSP_BUSY
        MMC_RSP_R2 = MMC_RSP_PRESENT | MMC_RSP_136 | MMC_RSP_CRC
        MMC_RSP_R3 = MMC_RSP_PRESENT
        MMC_RSP_R4 = MMC_RSP_PRESENT
        MMC_RSP_R5 = MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE
        MMC_RSP_R6 = MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE
        MMC_RSP_R7 = MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE

        MMC_CMD_MASK = 3 << 5
        MMC_CMD_AC = 0 << 5
        MMC_CMD_ADTC = 1 << 5
        MMC_CMD_BC = 2 << 5
        MMC_CMD_BCR = 3 << 5

        MMC_RSP_SPI_S1 = 1 << 7
        MMC_RSP_SPI_S2 = 1 << 8
        MMC_RSP_SPI_B4 = 1 << 9
        MMC_RSP_SPI_BUSY = 1 << 10

        MMC_RSP_SPI_R1 = MMC_RSP_SPI_S1
        MMC_RSP_SPI_R1B = MMC_RSP_SPI_S1 | MMC_RSP_SPI_BUSY
        MMC_RSP_SPI_R2 = MMC_RSP_SPI_S1 | MMC_RSP_SPI_S2
        MMC_RSP_SPI_R3 = MMC_RSP_SPI_S1 | MMC_RSP_SPI_B4
        MMC_RSP_SPI_R4 = MMC_RSP_SPI_S1 | MMC_RSP_SPI_B4
        MMC_RSP_SPI_R5 = MMC_RSP_SPI_S1 | MMC_RSP_SPI_S2
        MMC_RSP_SPI_R7 = MMC_RSP_SPI_S1 | MMC_RSP_SPI_B4

    RESP_NONE = 0
    RESP_R1 = 1
    RESP_R2 = 2
    RESP_R3 = 3
    RESP_R4 = 4
    RESP_R5 = 5
    RESP_R6 = 6
    RESP_R7 = 7
    RESP_R1B = 8
    msdc_rsp = [0, 1, 2, 3, 4, 1, 1, 1, 7]
    WINTS_CMD = (1 << 8) | (1 << 10) | (1 << 9) | (1 << 3) | (1 << 5) | (1 << 4)
    MSDC_FIFO_THD = 64
    MSDC_FIFO_SZ = 128

    class Reg(Enum):
        MSDC_CFG = 0x0
        MSDC_IOCON = 0x04
        MSDC_PS = 0x08
        MSDC_INT = 0x0c
        MSDC_INTEN = 0x10
        MSDC_FIFOCS = 0x14
        MSDC_TXDATA = 0x18
        MSDC_RXDATA = 0x1c
        SDC_CFG = 0x30
        SDC_CMD = 0x34
        SDC_ARG = 0x38
        SDC_STS = 0x3c
        SDC_RESP0 = 0x40
        SDC_RESP1 = 0x44
        SDC_RESP2 = 0x48
        SDC_RESP3 = 0x4c
        SDC_BLK_NUM = 0x50
        SDC_CSTS = 0x58
        SDC_CSTS_EN = 0x5c
        SDC_DCRC_STS = 0x60
        EMMC_CFG0 = 0x70
        EMMC_CFG1 = 0x74
        EMMC_STS = 0x78
        EMMC_IOCON = 0x7c
        SDC_ACMD_RESP = 0x80
        SDC_ACMD19_TRG = 0x84
        SDC_ACMD19_STS = 0x88
        MSDC_DMA_SA = 0x90
        MSDC_DMA_CA = 0x94
        MSDC_DMA_CTRL = 0x98
        MSDC_DMA_CFG = 0x9c
        MSDC_DBG_SEL = 0xa0
        MSDC_DBG_OUT = 0xa4
        MSDC_DMA_LEN = 0xa8
        MSDC_PATCH_BIT = 0xb0
        MSDC_PATCH_BIT1 = 0xb4
        DAT0_TUNE_CRC = 0xc0
        DAT1_TUNE_CRC = 0xc4
        DAT2_TUNE_CRC = 0xc8
        DAT3_TUNE_CRC = 0xcc
        CMD_TUNE_CRC = 0xd0
        SDIO_TUNE_WIND = 0xd4
        MSDC_PAD_CTL0 = 0xe0
        MSDC_PAD_CTL1 = 0xe4
        MSDC_PAD_CTL2 = 0xe8
        MSDC_PAD_TUNE = 0xec
        MSDC_DAT_RDDLY0 = 0xf0
        MSDC_DAT_RDDLY1 = 0xf4
        MSDC_HW_DBG = 0xf8
        MSDC_VERSION = 0x100
        MSDC_ECO_VER = 0x104

    class Vdd(IntFlag):
        MMC_VDD_165_195 = 0x00000080
        MMC_VDD_20_21 = 0x00000100
        MMC_VDD_21_22 = 0x00000200
        MMC_VDD_22_23 = 0x00000400
        MMC_VDD_23_24 = 0x00000800
        MMC_VDD_24_25 = 0x00001000
        MMC_VDD_25_26 = 0x00002000
        MMC_VDD_26_27 = 0x00004000
        MMC_VDD_27_28 = 0x00008000
        MMC_VDD_28_29 = 0x00010000
        MMC_VDD_29_30 = 0x00020000
        MMC_VDD_30_31 = 0x00040000
        MMC_VDD_31_32 = 0x00080000
        MMC_VDD_32_33 = 0x00100000
        MMC_VDD_33_34 = 0x00200000
        MMC_VDD_34_35 = 0x00400000
        MMC_VDD_35_36 = 0x00800000

    class Int(IntFlag):
        MSDC_INTEN_MMCIRQ = (0x1 << 0)
        MSDC_INTEN_CDSC = (0x1 << 1)
        MSDC_INTEN_ACMDRDY = (0x1 << 3)
        MSDC_INTEN_ACMDTMO = (0x1 << 4)
        MSDC_INTEN_ACMDCRCERR = (0x1 << 5)
        MSDC_INTEN_DMAQ_EMPTY = (0x1 << 6)
        MSDC_INTEN_SDIOIRQ = (0x1 << 7)
        MSDC_INTEN_CMDRDY = (0x1 << 8)
        MSDC_INTEN_CMDTMO = (0x1 << 9)
        MSDC_INTEN_RSPCRCERR = (0x1 << 10)
        MSDC_INTEN_CSTA = (0x1 << 11)
        MSDC_INTEN_XFER_COMPL = (0x1 << 12)
        MSDC_INTEN_DXFER_DONE = (0x1 << 13)
        MSDC_INTEN_DATTMO = (0x1 << 14)
        MSDC_INTEN_DATCRCERR = (0x1 << 15)
        MSDC_INTEN_ACMD19_DONE = (0x1 << 16)

    def reg_write(self, reg: Reg, value: int):
        self.device.write32(self.base + reg.value, [value])

    def reg_read(self, reg: Reg):
        return self.device.read32(self.base + reg.value)

    def reg_setbits(self, reg: Reg, bits: int):
        v = self.device.read32(self.base + reg.value)
        v |= bits
        self.device.write32(self.base + reg.value, [v])

    def reg_clrbits(self, reg: Reg, bits: int):
        v = self.device.read32(self.base + reg.value)
        v &= ~bits
        self.device.write32(self.base + reg.value, [v])

    def __init__(self, device, base, debug=False):
        self.device = device
        self.base = base
        self.debug = debug
        self.block_size = 512
        self.use_cmd23 = False
        self.cmd_resp = None

        # for MT8127 / MT8163 / MT8135
        self.msdc_ocr_avail = Mmc.Vdd.MMC_VDD_28_29 | Mmc.Vdd.MMC_VDD_29_30 | Mmc.Vdd.MMC_VDD_30_31 | Mmc.Vdd.MMC_VDD_31_32 | Mmc.Vdd.MMC_VDD_32_33

    def msdc_reset_hw(self):
        pass

    def __sdc_is_busy(self):
        if self.reg_read(Mmc.Reg.SDC_STS) & 1:
            return True
        else:
            return False

    def __sdc_cmd_is_busy(self):
        if self.reg_read(Mmc.Reg.SDC_STS) & (1 << 1):
            return True
        else:
            return False

    def __sdc_send_cmd(self, rawcmd, rawarg):
        self.reg_write(Mmc.Reg.SDC_ARG, rawarg)
        self.reg_write(Mmc.Reg.SDC_CMD, rawcmd)

    def __msdc_command_start(self, opcode: Opcode, arg: int, flags: Flags):
        resp = self.RESP_NONE
        cmd_type = flags & Mmc.Flags.MMC_CMD_MASK

        if opcode == Mmc.Opcode.MMC_SEND_OP_COND or opcode == Mmc.Opcode.SD_APP_OP_COND:
            resp = self.RESP_R3
        elif opcode == Mmc.Opcode.MMC_SET_RELATIVE_ADDR:
            if cmd_type == Mmc.Flags.MMC_CMD_BCR:
                resp = self.RESP_R6
            else:
                resp = self.RESP_R1
        elif opcode == Mmc.Opcode.MMC_FAST_IO:
            resp = self.RESP_R4
        elif opcode == Mmc.Opcode.MMC_GO_IRQ_STATE:
            resp = self.RESP_R6
        elif opcode == Mmc.Opcode.MMC_SELECT_CARD:
            if arg == 0:
                resp = self.RESP_NONE
            else:
                resp = self.RESP_R1B
        elif opcode == Mmc.Opcode.SD_SEND_IF_CONF and cmd_type == Mmc.Flags.MMC_CMD_BCR:
            resp = self.RESP_R1
        else:
            resp_type = flags & (Mmc.Flags.MMC_RSP_PRESENT | Mmc.Flags.MMC_RSP_136 |
                                 Mmc.Flags.MMC_RSP_CRC | Mmc.Flags.MMC_RSP_BUSY | Mmc.Flags.MMC_RSP_OPCODE)
            if resp_type == Mmc.Flags.MMC_RSP_R1:
                resp = self.RESP_R1
            elif resp_type == Mmc.Flags.MMC_RSP_R1B:
                resp = self.RESP_R1B
            elif resp_type == Mmc.Flags.MMC_RSP_R2:
                resp = self.RESP_R2
            elif resp_type == Mmc.Flags.MMC_RSP_R3:
                resp = self.RESP_R3
            else:
                resp = self.RESP_NONE

        rawcmd = opcode.value | (self.msdc_rsp[resp] << 7) | (self.block_size << 16)

        if opcode == Mmc.Opcode.MMC_READ_MULTIPLE_BLOCK:
            assert 0  # TODO
        elif opcode == Mmc.Opcode.MMC_READ_SINGLE_BLOCK:
            rawcmd |= 1 << 11
        elif opcode == Mmc.Opcode.MMC_WRITE_MULTIPLE_BLOCK:
            assert 0  # TODO
        elif opcode == Mmc.Opcode.MMC_WRITE_BLOCK:
            rawcmd |= (1 << 11) | (1 << 13)
        elif (opcode == Mmc.Opcode.MMC_SEND_EXT_CSD and cmd_type == Mmc.Flags.MMC_CMD_ADTC) or (
                opcode == Mmc.Opcode.MMC_SWITCH and cmd_type == Mmc.Flags.MMC_CMD_ADTC):
            rawcmd |= 1 << 11
        elif opcode == Mmc.Opcode.MMC_STOP_TRANSMISSION:
            rawcmd |= 1 << 14
            rawcmd &= ~(0x0FFF << 16)

        if opcode == Mmc.Opcode.MMC_SEND_STATUS:
            while self.__sdc_cmd_is_busy():
                pass
        else:
            while self.__sdc_is_busy():
                pass

        self.reg_clrbits(Mmc.Reg.MSDC_INTEN, self.WINTS_CMD)
        self.__sdc_send_cmd(rawcmd, arg)

        self.cmd_resp = resp

    def __msdc_command_resp_polling(self, opcode: Opcode, arg: int, flags: Flags):
        cmdsts = (1 << 8) | (1 << 10) | (1 << 9)
        while True:
            intsts = self.reg_read(Mmc.Reg.MSDC_INT)
            if intsts & cmdsts != 0:
                intsts &= cmdsts
                self.reg_write(Mmc.Reg.MSDC_INT, intsts)
                break

        error = 0
        rsp = []

        if intsts & cmdsts:
            if (intsts & (1 << 8)) or (intsts & (1 << 3)) or (intsts & (1 << 16)):
                if self.cmd_resp == self.RESP_NONE:
                    pass
                elif self.cmd_resp == self.RESP_R2:
                    rsp.append(self.reg_read(Mmc.Reg.SDC_RESP3))
                    rsp.append(self.reg_read(Mmc.Reg.SDC_RESP2))
                    rsp.append(self.reg_read(Mmc.Reg.SDC_RESP1))
                    rsp.append(self.reg_read(Mmc.Reg.SDC_RESP0))
                else:
                    rsp.append(self.reg_read(Mmc.Reg.SDC_RESP0))
            else:
                if intsts & (1 << 10):
                    error = 1
                    print('XXX CMD<%d> MSDC_INT_RSPCRCERR Arg<0x%.8x>' % (opcode.value, arg))
                    if opcode != Mmc.Opcode.MMC_SEND_TUNING_BLOCK:
                        self.msdc_reset_hw()
                elif intsts & (1 << 9):
                    error = 1
                    print('XXX CMD<%d> MSDC_INT_CMDTMO Arg<0x%.8x>' % (opcode.value, arg))
                    self.msdc_reset_hw()

        return (error, rsp)

    def __msdc_set_blknum(self, blknum):
        self.reg_write(Mmc.Reg.SDC_BLK_NUM, blknum)

    def __msdc_pio_read(self):
        wints = Mmc.Int.MSDC_INTEN_DATTMO | Mmc.Int.MSDC_INTEN_DATCRCERR | Mmc.Int.MSDC_INTEN_XFER_COMPL
        get_xfer_done = False
        ints = 0
        num = 1
        left = 0
        buffer = b''

        self.reg_clrbits(Mmc.Reg.MSDC_INTEN, wints)

        while True:
            if not get_xfer_done:
                ints = self.reg_read(Mmc.Reg.MSDC_INTEN)
                ints &= wints
                self.reg_write(Mmc.Reg.MSDC_INTEN, ints)
            if ints & (1 << 14):
                self.msdc_reset_hw()
                raise RuntimeError('Data timeout error')
            elif ints & (1 << 15):
                self.msdc_reset_hw()
                raise RuntimeError('Data CRC error')
            elif ints & (1 << 12):
                get_xfer_done = True
                if num == 0 and left == 0:
                    break

            if num == 0 and left == 0:
                continue

            left = 512
            while left > 0:
                assert left >= 4

                rxfifocnt = self.reg_read(Mmc.Reg.MSDC_FIFOCS) & 0xff
                if left >= self.MSDC_FIFO_THD and rxfifocnt >= self.MSDC_FIFO_THD:
                    count = self.MSDC_FIFO_THD // 4
                    while count > 0:
                        buffer += pack('<I', self.reg_read(Mmc.Reg.MSDC_RXDATA))
                        count -= 1
                    left -= self.MSDC_FIFO_THD
                elif left < self.MSDC_FIFO_THD and rxfifocnt >= left:
                    while left > 0:
                        buffer += pack('<I', self.reg_read(Mmc.Reg.MSDC_RXDATA))
                        left -= 4

                if left > 0:
                    ints = self.reg_read(Mmc.Reg.MSDC_INT)
                    if ints & (1 << 14):
                        raise RuntimeError('Data timeout error')
            break

        return buffer

    def __msdc_pio_write(self, buffer: bytes):
        assert len(buffer) == 512

        wints = Mmc.Int.MSDC_INTEN_DATTMO | Mmc.Int.MSDC_INTEN_DATCRCERR | Mmc.Int.MSDC_INTEN_XFER_COMPL
        get_xfer_done = False
        ints = 0
        num = 1
        left = 0

        self.reg_clrbits(Mmc.Reg.MSDC_INTEN, wints)

        while True:
            if not get_xfer_done:
                ints &= self.reg_read(Mmc.Reg.MSDC_INT)
                ints &= wints
                self.reg_write(Mmc.Reg.MSDC_INT, ints)
            if ints & (1 << 14):
                self.msdc_reset_hw()
                raise RuntimeError('Data timeout error')
            elif ints & (1 << 15):
                self.msdc_reset_hw()
                raise RuntimeError('Data CRC error')
            elif ints & (1 << 12):
                get_xfer_done = True
                if num == 0 and left == 0:
                    break

            if num == 0 and left == 0:
                continue

            left = 512
            while left > 0:
                assert left >= 4

                txfifocnt = (self.reg_read(Mmc.Reg.MSDC_FIFOCS) >> 16) & 0xff
                if left >= self.MSDC_FIFO_SZ and txfifocnt == 0:
                    count = self.MSDC_FIFO_SZ // 4
                    while count > 0:
                        self.reg_write(Mmc.Reg.MSDC_TXDATA, unpack('<I', buffer[0:4])[0])
                        buffer = buffer[4:]
                        count -= 1
                    left -= self.MSDC_FIFO_SZ
                elif left < self.MSDC_FIFO_SZ and txfifocnt == 0:
                    while left > 0:
                        self.reg_write(Mmc.Reg.MSDC_TXDATA, unpack('<I', buffer[0:4])[0])
                        buffer = buffer[4:]
                        left -= 4

            break

    def __uffs(self, x: int):
        r = 1

        if x == 0:
            return 0
        if x & 0xffff == 0:
            x >>= 16
            r += 16
        if x & 0xff == 0:
            x >>= 8
            r += 8
        if x & 0xf == 0:
            x >>= 4
            r += 4
        if x & 3 == 0:
            x >>= 2
            r += 2
        if x & 1 == 0:
            x >>= 1
            r += 1

        return r

    def __mmc_select_voltage(self, ocr: int):
        ocr &= self.msdc_ocr_avail
        bit = self.__uffs(ocr)
        if bit:
            bit -= 1
            ocr &= 3 << bit
        else:
            ocr = 0
        return ocr

    def mmc_command(self, opcode: Opcode, arg: int, flags: Flags):
        self.__msdc_command_start(opcode, arg, flags)
        return self.__msdc_command_resp_polling(opcode, arg, flags)

    def mmc_go_idle(self):
        self.mmc_command(Mmc.Opcode.MMC_GO_IDLE_STATE, 0,
                         Mmc.Flags.MMC_RSP_SPI_R1 | Mmc.Flags.MMC_RSP_NONE | Mmc.Flags.MMC_CMD_BC)

    def __mmc_send_op_cond(self, ocr):
        s = None
        for i in range(100):
            s = self.mmc_command(Mmc.Opcode.MMC_SEND_OP_COND, ocr,
                                 Mmc.Flags.MMC_RSP_SPI_R1 | Mmc.Flags.MMC_RSP_R3 | Mmc.Flags.MMC_CMD_BCR)
            if s[0]:
                break
            if ocr == 0:
                break
            if s[1][0] & 0x80000000:
                break

            s = (1, [])

            time.sleep(0.01)

        if s[0] == 1:
            return None
        else:
            return s[1][0]

    def __mmc_all_send_cid(self):
        e = self.mmc_command(Mmc.Opcode.MMC_ALL_SEND_CID, 0, Mmc.Flags.MMC_RSP_R2 | Mmc.Flags.MMC_CMD_BCR)
        assert e[0] == 0
        return e[1]

    def __mmc_set_relative_addr(self, rca):
        assert \
        self.mmc_command(Mmc.Opcode.MMC_SET_RELATIVE_ADDR, rca << 16, Mmc.Flags.MMC_RSP_R1 | Mmc.Flags.MMC_CMD_AC)[
            0] == 0

    def __mmc_select_card(self, rca):
        assert self.mmc_command(Mmc.Opcode.MMC_SELECT_CARD, rca << 16, Mmc.Flags.MMC_RSP_R1 | Mmc.Flags.MMC_CMD_AC)[
                   0] == 0

    def __mmc_send_status(self):
        return self.mmc_command(Mmc.Opcode.MMC_SEND_STATUS, 1 << 16,
                                Mmc.Flags.MMC_RSP_SPI_R2 | Mmc.Flags.MMC_RSP_R1 | Mmc.Flags.MMC_CMD_AC)

    def __mmc_switch(self, set, index, value, use_busy_signal=True):
        flags = Mmc.Flags.MMC_CMD_AC
        if use_busy_signal:
            flags |= Mmc.Flags.MMC_RSP_SPI_R1B | Mmc.Flags.MMC_RSP_R1B
        else:
            flags |= Mmc.Flags.MMC_RSP_SPI_R1 | Mmc.Flags.MMC_RSP_R1

        e = self.mmc_command(Mmc.Opcode.MMC_SWITCH, (3 << 24) | (index << 16) | (value << 8) | set, flags)
        if e[0]:
            return e

        status = 0

        if use_busy_signal:
            while True:
                e = self.__mmc_send_status()
                if e[0]:
                    return e
                status = e[1][0]

                if (status & 0x00001E00) >> 9 == 7:
                    continue
                else:
                    break

            if status & 0xFDFFA000:
                print('unexpected status %#08x after switch' % status, flush=True)
            if status & (1 << 7):
                print('switch error', flush=True)
                return 1

        return 0

    def mmc_set_part(self, part):
        assert self.__mmc_switch(1, 179, 72 | part) == 0

    def mmc_read_single_block(self, block):
        self.__msdc_set_blknum(1)
        assert self.mmc_command(Mmc.Opcode.MMC_READ_SINGLE_BLOCK, block, Mmc.Flags.MMC_RSP_R1 | Mmc.Flags.MMC_CMD_ADTC)[
                   0] == 0
        return self.__msdc_pio_read()

    def mmc_write_single_block(self, block, buffer: bytes):
        assert len(buffer) == 512
        self.__msdc_set_blknum(1)
        assert self.mmc_command(Mmc.Opcode.MMC_WRITE_BLOCK, block, Mmc.Flags.MMC_RSP_R1 | Mmc.Flags.MMC_CMD_ADTC)[
                   0] == 0
        self.__msdc_pio_write(buffer)