#include "rpi.h"
#include "emmc.h"
#include "mbox.h"

// Raspberry Pi EMMC driver adapted from Low Level Devel:
// https://github.com/rockytriton/LLD

static bool wait_reg_mask(reg32 *reg, u32 mask, bool set, u32 timeout) {
  for (int cycles = 0; cycles <= timeout * 10; cycles++) {
    if ((*reg & mask) ? set : !set) {
      return true;
    }

    delay_us(100);
  }

  return false;
}

static u32 get_clock_divider(u32 base_clock) {
#define TARGET_RATE SD_CLOCK_HIGH
  u32 target_div = 1;

  if (TARGET_RATE <= base_clock) {
    target_div = base_clock / TARGET_RATE;

    if (base_clock % TARGET_RATE) {
      target_div = 0;
    }
  }

  int div = -1;
  for (int fb = 31; fb >= 0; fb--) {
    u32 bt = (1 << fb);

    if (target_div & bt) {
      div = fb;
      target_div &= ~(bt);

      if (target_div) {
        div++;
      }

      break;
    }
  }

  if (div == -1) {
    div = 31;
  }

  if (div >= 32) {
    div = 31;
  }

  if (div != 0) {
    div = (1 << (div - 1));
  }

  if (div >= 0x400) {
    div = 0x3FF;
  }

  u32 freqSel = div & 0xff;
  u32 upper = (div >> 8) & 0x3;
  u32 ret = (freqSel << 8) | (upper << 6) | (0 << 5);

  return ret;
}

/* static bool switch_clock_rate(u32 base_clock, u32 target_rate) { */
/*   u32 divider = get_clock_divider(base_clock, target_rate); */

/*   while((EMMC->status & (EMMC_STATUS_CMD_INHIBIT | EMMC_STATUS_DAT_INHIBIT))) { */
/*     delay_ms(1); */
/*   } */

/*   u32 c1 = EMMC->control[1] & ~EMMC_CTRL1_CLK_ENABLE; */

/*   EMMC->control[1] = c1; */

/*   delay_ms(3); */

/*   EMMC->control[1] = (c1 | divider) & ~0xFFE0; */

/*   delay_ms(3); */

/*   EMMC->control[1] = c1 | EMMC_CTRL1_CLK_ENABLE; */

/*   delay_ms(3); */

/*   return true; */
/* } */

bool emmc_setup_clock() {
  EMMC->control2 = 0;

  /* u32 rate = mailbox_clock_rate(CT_EMMC); */
  /* u32 rate = rpi_clock_curhz_get(CLOCK_EMMC); */
  u32 rate = 250000000;

  u32 n = EMMC->control[1];
  n |= EMMC_CTRL1_CLK_INT_EN;
  n |= get_clock_divider(rate);
  n &= ~(0xf << 16);
  n |= (11 << 16);

  EMMC->control[1] = n;

  if (!wait_reg_mask(&EMMC->control[1], EMMC_CTRL1_CLK_STABLE, true, 2000)) {
    printk("EMMC_ERR: SD CLOCK NOT STABLE\n");
    return false;
  }

  delay_ms(30);

  //enabling the clock
  EMMC->control[1] |= 4;

  delay_ms(30);

  return true;
}

static emmc_device device = {0};

static const emmc_cmd INVALID_CMD = RES_CMD;

static const emmc_cmd commands[] = {
  {0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0},
  RES_CMD,
  {0, 0, 0, 0, 0, 0, RT136, 0, 1, 0, 0, 0, 2, 0},
  {0, 0, 0, 0, 0, 0, RT48,  0, 1, 0, 0, 0, 3, 0},
  {0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 4, 0},
  {0, 0, 0, 0, 0, 0, RT136, 0, 0, 0, 0, 0, 5, 0},
  {0, 0, 0, 0, 0, 0, RT48,  0, 1, 0, 0, 0, 6, 0},
  {0, 0, 0, 0, 0, 0, RT48Busy,  0, 1, 0, 0, 0, 7, 0},
  {0, 0, 0, 0, 0, 0, RT48,  0, 1, 0, 0, 0, 8, 0},
  {0, 0, 0, 0, 0, 0, RT136, 0, 1, 0, 0, 0, 9, 0},
  RES_CMD,
  RES_CMD,
  RES_CMD,
  RES_CMD,
  RES_CMD,
  RES_CMD,
  {0, 0, 0, 0, 0, 0, RT48,  0, 1, 0, 0, 0, 16, 0},
  {0, 0, 0, 1, 0, 0, RT48,  0, 1, 0, 1, 0, 17, 0},
  {0, 1, 1, 1, 1, 0, RT48,  0, 1, 0, 1, 0, 18, 0},
  RES_CMD,
  RES_CMD,
  RES_CMD,
  RES_CMD,
  RES_CMD,
  {0, 0, 0, 0, 0, 0, RT48,  0, 1, 0, 1, 0, 24, 0},
  {0, 1, 1, 0, 1, 0, RT48,  0, 1, 0, 1, 0, 25, 0},
  RES_CMD,
  RES_CMD,
  RES_CMD,
  RES_CMD,
  RES_CMD,
  RES_CMD,
  RES_CMD,
  RES_CMD,
  RES_CMD,
  RES_CMD,
  RES_CMD,
  RES_CMD,
  RES_CMD,
  RES_CMD,
  RES_CMD,
  {0, 0, 0, 0, 0, 0, RT48,  0, 0, 0, 0, 0, 41, 0},
  RES_CMD,
  RES_CMD,
  RES_CMD,
  RES_CMD,
  RES_CMD,
  RES_CMD,
  RES_CMD,
  RES_CMD,
  RES_CMD,
  {0, 0, 0, 1, 0, 0, RT48,  0, 1, 0, 1, 0, 51, 0},
  RES_CMD,
  RES_CMD,
  RES_CMD,
  {0, 0, 0, 0, 0, 0, RT48,  0, 1, 0, 0, 0, 55, 0},
};

static u32 sd_error_mask(sd_error err) {
  return 1 << (16 + (u32)err);
}

static void set_last_error(u32 intr_val) {
  device.last_error = intr_val & 0xFFFF0000;
  device.last_interrupt = intr_val;
}

static bool do_data_transfer(emmc_cmd cmd) {
  u32 wrIrpt = 0;
  bool write = false;

  if (cmd.direction) {
    wrIrpt = 1 << 5;
  } else {
    wrIrpt = 1 << 4;
    write = true;
  }

  u32 *data = (u32 *)device.buffer;

  for (int block = 0; block < device.transfer_blocks; block++) {
    wait_reg_mask(&EMMC->int_flags, wrIrpt | 0x8000, true, 2000);
    u32 intr_val = EMMC->int_flags;
    EMMC->int_flags = wrIrpt | 0x8000;

    if ((intr_val & (0xffff0000 | wrIrpt)) != wrIrpt) {
      set_last_error(intr_val);
      return false;
    }


    u32 length = device.block_size;

    if (write) {
      for (; length > 0; length -= 4) {
        EMMC->data = *data++;
      }
    } else {
      for (; length > 0; length -= 4) {
        *data++ = EMMC->data;
      }
    }
  }

  return true;
}

static bool emmc_issue_command(emmc_cmd cmd, u32 arg, u32 timeout) {
  device.last_command_value = TO_REG(&cmd);
  reg32 command_reg = device.last_command_value;

  if (device.transfer_blocks > 0xFFFF) {
    printk("EMMC_ERR: transferBlocks too large: %d\n", device.transfer_blocks);
    return false;
  }

  EMMC->block_size_count = device.block_size | (device.transfer_blocks << 16);
  EMMC->arg1 = arg;
  EMMC->cmd_xfer_mode = command_reg;

  int times = 0;

  while(times < timeout) {
    u32 reg = EMMC->int_flags;

    if (reg & 0x8001) {
      break;
    }

    delay_ms(1);
    times++;
  }

  if (times >= timeout) {
    //just doing a warn for this because sometimes it's ok.
    printk("EMMC_WARN: emmc_issue_command timed out\n");
    device.last_success = false;
    return false;
  }

  u32 intr_val = EMMC->int_flags;

  EMMC->int_flags = 0xFFFF0001;

  if ((intr_val & 0xFFFF0001) != 1) {

    if (EMMC_DEBUG) printk("EMMC_DEBUG: Error waiting for command interrupt complete: %d\n", cmd.index);

    set_last_error(intr_val);

    if (EMMC_DEBUG) printk("EMMC_DEBUG: IRQFLAGS: %x - %x - %x\n", EMMC->int_flags, EMMC->status, intr_val);

    device.last_success = false;
    return false;
  }

  switch(cmd.response_type) {
    case RT48:
    case RT48Busy:
      device.last_response[0] = EMMC->response[0];
      break;

    case RT136:
      device.last_response[0] = EMMC->response[0];
      device.last_response[1] = EMMC->response[1];
      device.last_response[2] = EMMC->response[2];
      device.last_response[3] = EMMC->response[3];
      break;
  }

  if (cmd.is_data) {
    do_data_transfer(cmd);
  }

  if (cmd.response_type == RT48Busy || cmd.is_data) {
    wait_reg_mask(&EMMC->int_flags, 0x8002, true, 2000);
    intr_val = EMMC->int_flags;

    EMMC->int_flags = 0xFFFF0002;

    if ((intr_val & 0xFFFF0002) != 2 && (intr_val & 0xFFFF0002) != 0x100002) {
      set_last_error(intr_val);
      return false;
    }

    EMMC->int_flags = 0xFFFF0002;
  }

  device.last_success = true;

  return true;
}

static bool emmc_command(u32 command, u32 arg, u32 timeout) {
  if (command & 0x80000000) {
    //The app command flag is set, shoudl use emmc_app_command instead.
    printk("EMMC_ERR: COMMAND ERROR NOT APP\n");
    return false;
  }

  device.last_command = commands[command];

  if (TO_REG(&device.last_command) == TO_REG(&INVALID_CMD)) {
    printk("EMMC_ERR: INVALID COMMAND!\n");
    return false;
  }

  return emmc_issue_command( device.last_command, arg, timeout);
}

static bool reset_command() {
  EMMC->control[1] |= EMMC_CTRL1_RESET_CMD;

  for (int i=0; i<10000; i++) {
    if (!( EMMC->control[1] & EMMC_CTRL1_RESET_CMD)) {
      return true;
    }

    delay_ms(1);
  }

  printk("EMMC_ERR: Command line failed to reset properly: %x\n",  EMMC->control[1]);

  return false;
}

bool emmc_app_command(u32 command, u32 arg, u32 timeout) {

  if (commands[command].index >= 60) {
    printk("EMMC_ERR: INVALID APP COMMAND\n");
    return false;
  }

  device.last_command = commands[CTApp];

  u32 rca = 0;

  if (device.rca) {
    rca = device.rca << 16;
  }

  if (emmc_issue_command( device.last_command, rca, 2000)) {
    device.last_command = commands[command];

    return emmc_issue_command( device.last_command, arg, 2000);
  }

  return false;
}

static bool check_v2_card() {
  bool v2Card = false;

  if (!emmc_command( CTSendIfCond, 0x1AA, 200)) {
    if (device.last_error == 0) {
      //timeout.
      printk("EMMC_ERR: SEND_IF_COND Timeout\n");
    } else if (device.last_error & (1 << 16)) {
      //timeout command error
      if (!reset_command()) {
        return false;
      }

      EMMC->int_flags = sd_error_mask(SDECommandTimeout);
      printk("EMMC_ERR: SEND_IF_COND CMD TIMEOUT\n");
    } else {
      printk("EMMC_ERR: Failure sending SEND_IF_COND\n");
      return false;
    }
  } else {
    if ((device.last_response[0] & 0xFFF) != 0x1AA) {
      printk("EMMC_ERR: Unusable SD Card: %x\n", device.last_response[0]);
      return false;
    }

    v2Card = true;
  }

  return v2Card;
}

static bool check_usable_card() {
  if (!emmc_command( CTIOSetOpCond, 0, 1000)) {
    if (device.last_error == 0) {
      //timeout.
      printk("EMMC_ERR: CTIOSetOpCond Timeout\n");
    } else if (device.last_error & (1 << 16)) {
      //timeout command error
      //this is a normal expected error and calling the reset command will fix it.
      if (!reset_command()) {
        return false;
      }

      EMMC->int_flags = sd_error_mask(SDECommandTimeout);
    } else {
      printk("EMMC_ERR: SDIO Card not supported\n");
      return false;
    }
  }

  return true;
}

static bool check_sdhc_support(bool v2_card) {
  bool card_busy = true;

  while(card_busy) {
    u32 v2_flags = 0;

    if (v2_card) {
      v2_flags |= (1 << 30); //SDHC Support
    }

    if (!emmc_app_command( CTOcrCheck, 0x00FF8000 | v2_flags, 2000)) {
      printk("EMMC_ERR: APP CMD 41 FAILED 2nd\n");
      return false;
    }

    if (device.last_response[0] >> 31 & 1) {
      device.ocr = (device.last_response[0] >> 8 & 0xFFFF);
      device.sdhc = ((device.last_response[0] >> 30) & 1) != 0;
      card_busy = false;
    } else {
      if (EMMC_DEBUG) printk("EMMC_DEBUG: SLEEPING: %x\n", device.last_response[0]);
      delay_ms(500);
    }
  }

  return true;
}

static bool check_ocr() {
  bool passed = false;

  for (int i=0; i<5; i++) {
    if (!emmc_app_command(CTOcrCheck, 0, 2000)) {
      printk("EMMC_WARN: APP CMD OCR CHECK TRY %d FAILED\n", i + 1);
      passed = false;
    } else {
      passed = true;
    }

    if (passed) {
      break;
    }

    return false;
  }

  if (!passed) {
    printk("EMMC_ERR: APP CMD 41 FAILED\n");
    return false;
  }

  device.ocr = (device.last_response[0] >> 8 & 0xFFFF);

  if (EMMC_DEBUG) printk("MEMORY OCR: %x\n", device.ocr);

  return true;
}

static bool check_rca() {
  if (!emmc_command( CTSendCide, 0, 2000)) {
    printk("EMMC_ERR: Failed to send CID\n");

    return false;
  }

  if (EMMC_DEBUG) printk("EMMC_DEBUG: CARD ID: %x.%x.%x.%x\n", device.last_response[0], device.last_response[1], device.last_response[2], device.last_response[3]);

  if (!emmc_command( CTSendRelativeAddr, 0, 2000)) {
    printk("EMMC_ERR: Failed to send Relative Addr\n");

    return false;
  }

  device.rca = (device.last_response[0] >> 16) & 0xFFFF;

  if (EMMC_DEBUG) {
    printk("EMMC_DEBUG: RCA: %x\n", device.rca);

    printk("EMMC_DEBUG: CRC_ERR: %d\n", (device.last_response[0] >> 15) & 1);
    printk("EMMC_DEBUG: CMD_ERR: %d\n", (device.last_response[0] >> 14) & 1);
    printk("EMMC_DEBUG: GEN_ERR: %d\n", (device.last_response[0] >> 13) & 1);
    printk("EMMC_DEBUG: STS_ERR: %d\n", (device.last_response[0] >> 9) & 1);
    printk("EMMC_DEBUG: READY  : %d\n", (device.last_response[0] >> 8) & 1);
  }

  if (!((device.last_response[0] >> 8) & 1)) {
    printk("EMMC_ERR: Failed to read RCA\n");
    return false;
  }

  return true;
}

static bool select_card() {
  if (!emmc_command( CTSelectCard, device.rca << 16, 2000)) {
    printk("EMMC_ERR: Failed to select card\n");
    return false;
  }

  if (EMMC_DEBUG) printk("EMMC_DEBUG: Selected Card\n");

  u32 status = (device.last_response[0] >> 9) & 0xF;

  if (status != 3 && status != 4) {
    printk("EMMC_ERR: Invalid Status: %d\n", status);
    return false;
  }

  if (EMMC_DEBUG) printk("EMMC_DEBUG: Status: %d\n", status);

  return true;
}

static bool set_scr() {
  if (!device.sdhc) {
    if (!emmc_command( CTSetBlockLen, 512, 2000)) {
      printk("EMMC_ERR: Failed to set block len\n");
      return false;
    }
  }

  u32 bsc = EMMC->block_size_count;
  bsc &= ~0xFFF; //mask off bottom bits
  bsc |= 0x200; //set bottom bits to 512
  EMMC->block_size_count = bsc;

  device.buffer = &device.scr.scr[0];
  device.block_size = 8;
  device.transfer_blocks = 1;

  if (!emmc_app_command( CTSendSCR, 0, 30000)) {
    printk("EMMC_ERR: Failed to send SCR\n");
    return false;
  }

  if (EMMC_DEBUG) printk("EMMC_DEBUG: GOT SRC: SCR0: %x SCR1: %x BWID: %x\n", device.scr.scr[0], device.scr.scr[1], device.scr.bus_widths);

  device.block_size = 512;

  u32 scr0 = BSWAP32(device.scr.scr[0]);
  device.scr.version = 0xFFFFFFFF;
  u32 spec = (scr0 >> (56 - 32)) & 0xf;
  u32 spec3 = (scr0 >> (47 - 32)) & 0x1;
  u32 spec4 = (scr0 >> (42 - 32)) & 0x1;

  if (spec == 0) {
    device.scr.version = 1;
  } else if (spec == 1) {
    device.scr.version = 11;
  } else if (spec == 2) {

    if (spec3 == 0) {
      device.scr.version = 2;
    } else if (spec3 == 1) {
      if (spec4 == 0) {
        device.scr.version = 3;
      }
      if (spec4 == 1) {
        device.scr.version = 4;
      }
    }
  }

  if (EMMC_DEBUG) printk("EMMC_DEBUG: SCR Version: %d\n", device.scr.version);

  return true;
}

static bool emmc_card_reset() {
  EMMC->control[1] = EMMC_CTRL1_RESET_HOST;

  if (EMMC_DEBUG) printk("EMMC_DEBUG: Card resetting...\n");

  if (!wait_reg_mask(&EMMC->control[1], EMMC_CTRL1_RESET_ALL, false, 2000)) {
    printk("EMMC_ERR: Card reset timeout!\n");
    return false;
  }

  /* #if (RPI_VERSION == 4) */
  /* //This enabled VDD1 bus power for SD card, needed for RPI 4. */
  /* u32 c0 = EMMC->control[0]; */
  /* c0 |= 0x0F << 8; */
  /* EMMC->control[0] = c0; */
  /* delay_ms(3); */
  /* #endif */

  if (!emmc_setup_clock()) {
    return false;
  }

  //All interrupts go to interrupt register.
  EMMC->int_enable = 0;
  EMMC->int_flags = 0xFFFFFFFF;
  EMMC->int_mask = 0xFFFFFFFF;

  delay_ms(203);

  device.transfer_blocks = 0;
  device.last_command_value = 0;
  device.last_success = false;
  device.block_size = 0;

  if (!emmc_command(CTGoIdle, 0, 2000)) {
    printk("EMMC_ERR: NO GO_IDLE RESPONSE\n");
    return false;
  }

  bool v2_card = check_v2_card();    

  if (!check_usable_card()) {
    return false;
  }

  if (!check_ocr()) {
    return false;
  }

  if (!check_sdhc_support(v2_card)) {
    return false;
  }

  // we increase the clock rate earlier so this is not necessary
  /* switch_clock_rate(device.base_clock, SD_CLOCK_NORMAL); */

  delay_ms(10);

  if (!check_rca()) {
    return false;
  }

  if (!select_card()) {
    return false;
  }

  if (!set_scr()) {
    return false;
  }

  //enable all interrupts
  EMMC->int_flags = 0xFFFFFFFF;

  if (EMMC_DEBUG) printk("EMMC_DEBUG: Card reset!\n");

  return true;
}

static bool do_data_command(bool write, u8 *b, u32 bsize, u32 block_no) {
  if (!device.sdhc) {
    block_no *= 512;
  }

  if (bsize < device.block_size) {
    printk("EMMC_ERR: INVALID BLOCK SIZE: \n", bsize, device.block_size);
    return false;
  }

  assert(device.block_size == 512);
  /* device.transfer_blocks = bsize / device.block_size; */
  device.transfer_blocks = bsize / 512;

  /* if (bsize % device.block_size) { */
  if (bsize & 0x1ff) {
    printk("EMMC_ERR: BAD BLOCK SIZE\n");
    return false;
  }

  device.buffer = b;

  cmd_type command = CTReadBlock;

  if (write && device.transfer_blocks > 1) {
    command = CTWriteMultiple;
  } else if (write) {
    command = CTWriteBlock;
  } else if (!write && device.transfer_blocks > 1) {
    command = CTReadMultiple;
  } 

  int retry_count = 0;
  int max_retries = 3;

  if (EMMC_DEBUG) printk("EMMC_DEBUG: Sending command: %d\n", command);

  while(retry_count < max_retries) {
    if (emmc_command( command, block_no, 5000)) {
      break;
    }

    if (++retry_count < max_retries) {
      printk("EMMC_WARN: Retrying data command\n");
    } else {
      printk("EMMC_ERR: Giving up data command\n");
      return false;
    }
  }

  return true;
}

int emmc_read(u32 sector, u8 *buffer, u32 size) {
  assert(size % 512 == 0);

  // this returns a bool, but it's used as an int???
  /* int r = do_data_command(false, buffer, size, sector); */
  /* if (r != size) { */
  /*   printk("EMMC_ERR: READ FAILED: %d\n", r); */
  /*   return -1; */
  /* } */

  bool success = do_data_command(false, buffer, size, sector);
  if (!success) {
    printk("EMMC_ERR: READ FAILED: sector=%d, size=%d\n", sector, size);
    return -1;
  }

  return size;
}

int emmc_write(u32 sector, u8 *buffer, u32 size) {
  assert(size % 512 == 0);

  int r = do_data_command(true, buffer, size, sector);
  if (!r) {
    printk("EMMC_ERR: WRITE FAILED: %d\n", r);
    return -1;
  }
  return size;
}

bool emmc_init() {
  gpio_set_function(34, GPIO_FUNC_INPUT);
  gpio_set_function(35, GPIO_FUNC_INPUT);
  gpio_set_function(36, GPIO_FUNC_INPUT);
  gpio_set_function(37, GPIO_FUNC_INPUT);
  gpio_set_function(38, GPIO_FUNC_INPUT);
  gpio_set_function(39, GPIO_FUNC_INPUT);

  gpio_set_function(48, GPIO_FUNC_ALT3);
  gpio_set_function(49, GPIO_FUNC_ALT3);
  gpio_set_function(50, GPIO_FUNC_ALT3);
  gpio_set_function(51, GPIO_FUNC_ALT3);
  gpio_set_function(52, GPIO_FUNC_ALT3);

  device.transfer_blocks = 0;
  device.last_command_value = 0;
  device.last_success = false;
  device.block_size = 0;
  device.sdhc = false;
  device.ocr = 0;
  device.rca = 0;
  device.base_clock = 0;

  bool success = false;
  for (int i=0; i<10; i++) {
    success = emmc_card_reset();

    if (success) {
      break;
    }

    delay_ms(100);
    printk("EMMC_WARN: Failed to reset card, trying again...\n");
  }

  if (!success) {
    return false;
  }

  return true;
}
