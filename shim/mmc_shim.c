/* Puerto del driver eMMC del vince (parche 0091) al shim del Daisy. Solo lectura.
 * Solo se conserva la parte de hardware: comandos, lectura de bloques y tabla GPT.
 * Los puntos de entrada de XNU (devfs, buf, bloqueos) se han quitado. */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include "console.h"

typedef uintptr_t vm_offset_t;
typedef int dev_t;
#define __unused
#define DEV_BSIZE 512
#define ENXIO 6
#define EIO 5
#define ENOENT 2
#define ETIMEDOUT 60
#define EINVAL 22
#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

/* Reloj del sistema: CNTPCT_EL0 a 19,2 MHz (igual que el mapa del Daisy). */
static inline uint64_t mach_absolute_time(void) {
#if defined(__aarch64__)
    uint64_t v; __asm__ volatile("mrs %0, cntpct_el0" : "=r"(v)); return v;
#else
    return 0;
#endif
}
static void delay(uint32_t us) {
    uint64_t end = mach_absolute_time() + (uint64_t)us * 192u / 10u;
    while (mach_absolute_time() < end) { }
}

static void *shim_memcpy(void *d, const void *s, size_t n) {
    uint8_t *dd = d; const uint8_t *ss = s; while (n--) *dd++ = *ss++; return d;
}
static void *shim_memset(void *d, int c, size_t n) {
    uint8_t *dd = d; while (n--) *dd++ = (uint8_t)c; return d;
}
static int shim_memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *x = a, *y = b; for (size_t i = 0; i < n; i++) if (x[i] != y[i]) return x[i] - y[i]; return 0;
}
static int shim_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; } return (uint8_t)*a - (uint8_t)*b;
}
#define memcpy shim_memcpy
#define memset shim_memset
#define memcmp shim_memcmp
#define strcmp shim_strcmp

/* printf mínimo: %s %u %d %x %llx %08x %% (en un buffer y a la pantalla). */
static void mlog(const char *fmt, ...) {
    char out[160]; size_t o = 0;
    va_list ap; va_start(ap, fmt);
    for (const char *p = fmt; *p && o < sizeof(out) - 1; p++) {
        if (*p != '%') { out[o++] = *p; continue; }
        p++;
        int zero = 0, width = 0, ll = 0;
        if (*p == '0') { zero = 1; p++; }
        while (*p >= '0' && *p <= '9') { width = width * 10 + (*p - '0'); p++; }
        if (*p == 'l') { p++; if (*p == 'l') { ll = 1; p++; } }
        char c = *p;
        if (c == '%') { out[o++] = '%'; continue; }
        if (c == 's') {
            const char *s = va_arg(ap, const char *); if (!s) s = "(null)";
            while (*s && o < sizeof(out) - 1) out[o++] = *s++;
            continue;
        }
        uint64_t v = ll ? va_arg(ap, unsigned long long) : (uint64_t)va_arg(ap, unsigned int);
        unsigned base = (c == 'x') ? 16u : 10u;
        char tmp[24]; int n = 0;
        do { unsigned dgt = (unsigned)(v % base); tmp[n++] = (char)(dgt < 10 ? '0' + dgt : 'a' + dgt - 10); v /= base; } while (v && n < 22);
        while (n < width && o < sizeof(out) - 1) out[o++] = zero ? '0' : ' ', width--;
        while (n) out[o++] = tmp[--n];
    }
    out[o] = 0;
    va_end(ap);
    con_str("MMC", out);
}

/*
 * VINCE internal eMMC root device (read-only).
 *
 * SDCC1 on the MSM8953 is an SDHCI 3.0 host (0x07824900) behind Qualcomm's MCI core (0x07824000). LK leaves it
 * unusable: the card asleep (CMD5), the core switched back to MCI mode, VENDOR_SPEC forcing HS400 with the DLL
 * in reset, and the card clock (the GCC SDCC1 apps RCG; the MSM SDHCI ignores its own divider) at 384 MHz. So this
 * driver re-initialises host and card from scratch, the sequence proven with the shim's monitor
 * (vince-bringup/shim/mmc.c, 2026-10-24):
 *   GCC RCG to 50 MHz, MCI core reset, HC_MODE, power-request acks, VENDOR_SPEC (no HS400 select, default MCLK,
 *   1.8 V pads), 1.8 V signalling, 400 kHz with a delay after every SDHCI write (the host runs on the card clock),
 *   CMD0 CMD1 CMD2 CMD3 CMD7, CMD6 to 8-bit and High Speed (volatile EXT_CSD bytes only), 50 MHz.
 * Data moves by PIO through the buffer data port, polled, one command at a time; no DMA, no interrupts.
 *
 * It finds one GPT partition by name (boot-arg mmcroot=, default "userdata") and exposes it as the BSD disk
 * mmcroot0 (rmmcroot0) with 512-byte sectors. Root on it with rd=mmcroot0. mmcprobe=1 (with any other root) only
 * initialises it and logs the partition and the filesystem signature found on it. Writes are refused, and no
 * command that writes the card's data (CMD24/25) exists here.
 */














/* osfmk/arm/machine_routines.h, not exported to the BSD layer */


#define SDCC1_PHYS      0x07824000ul    /* MCI core; the SDHCI registers start at +0x900 */
#define GCC_SDCC1_PHYS  0x01842000ul    /* SDCC1 BCR, then the apps RCG at +4 */
#define HC              0x900u

/* MCI core */
#define MCI_POWER       0x000           /* bit 7: software reset of the whole SDCC */
#define MCI_STATUS2     0x06c           /* bit 0: MCI active */
#define MCI_HC_MODE     0x078           /* bit 0: SDHCI mode, bit 13: FF_CLK_SW_RST_DIS */
#define PWRCTL_STATUS   0x0dc           /* bus off/on, IO low/high requests */
#define PWRCTL_CLEAR    0x0e4
#define PWRCTL_CTL      0x0e8           /* bit 0: bus success, bit 2: IO success */

/* SDHCI, relative to HC */
#define BLKSZ           0x04            /* block size | block count << 16 */
#define ARG             0x08
#define XFER            0x0c            /* transfer mode | command << 16 */
#define RESP0           0x10
#define BUF             0x20
#define PSTATE          0x24
#define HOSTCTL         0x28            /* host control 1 | power << 8 */
#define CLKCTL          0x2c            /* clock | timeout << 16 | software reset << 24 */
#define INTSTS          0x30            /* normal | error << 16 */
#define INTEN           0x34
#define SIGEN           0x38
#define HOSTCTL2        0x3c            /* auto CMD12 error | host control 2 << 16 */
#define VENDOR_SPEC     0x10c           /* MSM vendor register, on the SDHCI base for the v4 core too */

#define VS_MCLK_MASK    (3u << 8)
#define VS_MCLK_DFLT    (2u << 8)
#define VS_IO_PAD_EN    (1u << 15)
#define VS_IO_PAD_18V   (1u << 16)
#define VS_SELECT_IN    (0xfu << 18)    /* HC_SELECT_IN_EN (18) and HC_SELECT_IN (21:19) */

#define PS_CMD_INHIBIT  (1u << 0)
#define PS_DAT_INHIBIT  (1u << 1)
#define INT_CMD         (1u << 0)
#define INT_XFER        (1u << 1)
#define INT_RDREADY     (1u << 5)
#define INT_ERR         (1u << 15)
#define CLK_INT_EN      (1u << 0)
#define CLK_STABLE      (1u << 1)
#define CLK_SD_EN       (1u << 2)
#define TIMEOUT_MAX     (0xeu << 16)
#define RST_CMD         (1u << 25)
#define RST_DAT         (1u << 26)
#define HC_HISPEED      (1u << 2)
#define HC_8BIT         (1u << 5)
#define PWR_18V_ON      (0x0bu << 8)
#define HC2_18V         (1u << (16 + 3))

#define XM_BLKCNT       (1u << 1)
#define XM_ACMD12       (1u << 2)
#define XM_READ         (1u << 4)
#define XM_MULTI        (1u << 5)

#define C_R136          0x01
#define C_R48           0x02
#define C_R48B          0x03
#define C_CRC           0x08
#define C_IDX           0x10
#define C_DATA          0x20
#define F_R1            (C_R48 | C_CRC | C_IDX)
#define F_R1B           (C_R48B | C_CRC | C_IDX)
#define F_R2            (C_R136 | C_CRC)
#define F_R3            C_R48

#define RCA             2u
#define OCR_SECTOR_18V  0x40ff8080u     /* sector mode, 2.7-3.6 V and 1.70-1.95 V */
#define R1_ERR          0xfd380080u     /* status errors that belong to the command itself */

/* GCC RCG settings (gcc-msm8953 ftbl): cfg = mode << 12 | src << 8 | (2 * div - 1), then M/N */
#define RCG_400K        0x2017u, 1u, 4u         /* XO 19.2 MHz / 12 * 1/4 */
#define RCG_50M         0x011fu, 0u, 0u         /* GPLL0 800 MHz / 16 */

#define SECTOR          512u
#define MAX_SECTORS     64u             /* per CMD18, = the bounce buffer */
#define MAX_IO          (1u << 20)      /* what the BSD layer may ask for at once */

static struct {
	vm_offset_t base;               /* SDCC1, core at +0, SDHCI at +HC */
	vm_offset_t gcc;
	uint32_t wdelay_us;             /* after each SDHCI write while the card clock is slow */
	uint32_t resp;
	uint32_t err;                   /* interrupt status (or the polled register) at the last failure */
	uint64_t first, nsect;          /* partition, in 512-byte sectors */
	uint32_t secsize;               /* logical sector size for the BSD layer */
	int bmajor, cmajor;
	dev_t bdev;
	bool ready;
} vm;

static uint32_t vince_mmc_bounce[MAX_SECTORS * SECTOR / 4];


static inline uint32_t
hc_rd(uint32_t off)
{
	return *(volatile uint32_t *)(vm.base + HC + off);
}

static inline void
hc_wr(uint32_t off, uint32_t v)
{
	*(volatile uint32_t *)(vm.base + HC + off) = v;
	if (vm.wdelay_us) {
		delay(vm.wdelay_us);
	}
}

static inline uint32_t
core_rd(uint32_t off)
{
	return *(volatile uint32_t *)(vm.base + off);
}

static inline void
core_wr(uint32_t off, uint32_t v)
{
	*(volatile uint32_t *)(vm.base + off) = v;
}

static inline void
rcg_wr(uint32_t i, uint32_t v)
{
	((volatile uint32_t *)(vm.gcc + 4))[i] = v;    /* CMD_RCGR, CFG, M, N, D */
}

static uint64_t
deadline_ms(uint32_t ms)
{
	uint64_t t;
	t = (uint64_t)ms * 19200u;
	return mach_absolute_time() + t;
}

/* (*reg & mask) == want within ms; 0 or ETIMEDOUT (err = the last value) */
static int
mmc_poll(volatile uint32_t *reg, uint32_t mask, uint32_t want, uint32_t ms)
{
	uint64_t end = deadline_ms(ms);
	for (;;) {
		uint32_t v = *reg;
		if ((v & mask) == want) {
			return 0;
		}
		if (mach_absolute_time() > end) {
			vm.err = v;
			return ETIMEDOUT;
		}
	}
}

#define HCREG(o)        ((volatile uint32_t *)(vm.base + HC + (o)))
#define COREREG(o)      ((volatile uint32_t *)(vm.base + (o)))

static int
wait_int(uint32_t mask, uint32_t ms)
{
	uint64_t end = deadline_ms(ms);
	for (;;) {
		uint32_t s = hc_rd(INTSTS);
		if (s & INT_ERR) {
			vm.err = s;
			return EIO;
		}
		if (s & mask) {
			hc_wr(INTSTS, s & mask);
			return 0;
		}
		if (mach_absolute_time() > end) {
			vm.err = s | 0x8000u;           /* bit 15 without error bits: our own timeout */
			return ETIMEDOUT;
		}
	}
}

/* The MSM core asks for bus power / IO voltage changes to be confirmed (Linux does it from its pwr_irq). */
static void
pwrctl_ack(void)
{
	uint32_t s = core_rd(PWRCTL_STATUS);
	if (s) {
		core_wr(PWRCTL_CLEAR, s);
		core_wr(PWRCTL_CTL, ((s & 3) ? 1u : 0) | ((s & 0xc) ? 4u : 0));
	}
}

/* After any error SDHCI wants its CMD and DAT state machines reset. */
static void
recover(void)
{
	hc_wr(INTSTS, ~0u);
	hc_wr(CLKCTL, hc_rd(CLKCTL) | RST_CMD | RST_DAT);
	(void)mmc_poll(HCREG(CLKCTL), RST_CMD | RST_DAT, 0, 100);
	hc_wr(INTSTS, ~0u);
}

static int
mmc_cmd(uint32_t idx, uint32_t arg, uint32_t flags, uint32_t xfer, uint32_t blocks)
{
	uint32_t busy = PS_CMD_INHIBIT | ((flags & C_DATA) || (flags & 3) == C_R48B ? PS_DAT_INHIBIT : 0);
	int error = mmc_poll(HCREG(PSTATE), busy, 0, 100);
	if (error) {
		return error;
	}
	hc_wr(INTSTS, ~0u);
	if (flags & C_DATA) {
		hc_wr(BLKSZ, SECTOR | (blocks << 16));
	}
	hc_wr(ARG, arg);
	hc_wr(XFER, xfer | ((idx << 8 | flags) << 16));
	error = wait_int(INT_CMD, 100);
	if (error) {
		return error;
	}
	vm.resp = hc_rd(RESP0);
	if ((flags & 3) == C_R48B) {
		error = wait_int(INT_XFER, 1000);
	}
	return error;
}

static int
mmc_state(void)
{
	if (mmc_cmd(13, RCA << 16, F_R1, 0, 0)) {
		recover();
		return -1;
	}
	return (int)((vm.resp >> 9) & 0xf);
}

static int
rcg_set(uint32_t cfg, uint32_t m, uint32_t n)
{
	rcg_wr(1, cfg);
	if (n) {
		rcg_wr(2, m);
		rcg_wr(3, ~(n - m) & 0xff);
		rcg_wr(4, ~n & 0xff);
	}
	rcg_wr(0, 1);                                   /* UPDATE */
	return mmc_poll((volatile uint32_t *)(vm.gcc + 4), 1, 0, 10);
}

static int
set_clock(uint32_t cfg, uint32_t m, uint32_t n, uint32_t wdelay_us)
{
	hc_wr(CLKCTL, TIMEOUT_MAX | CLK_INT_EN);        /* card clock off while the RCG switches */
	if (rcg_set(cfg, m, n)) {
		return EIO;
	}
	vm.wdelay_us = wdelay_us;
	if (mmc_poll(HCREG(CLKCTL), CLK_STABLE, CLK_STABLE, 20)) {
		return EIO;
	}
	hc_wr(CLKCTL, TIMEOUT_MAX | CLK_INT_EN | CLK_SD_EN);
	delay(100);
	return 0;
}

/* CMD6 WRITE_BYTE of a volatile EXT_CSD byte, then CMD13 for SWITCH_ERROR */
static int
switch_byte(uint32_t index, uint32_t value)
{
	if (mmc_cmd(6, 3u << 24 | index << 16 | value << 8, F_R1B, 0, 0)) {
		return EIO;
	}
	return (mmc_state() == 4 && !(vm.resp & 0x80)) ? 0 : EIO;
}

static int
mmc_card_init(void)
{
	int step = 0;
	uint64_t end;
#define STEP(n, cond) do { step = (n); if (cond) goto fail; } while (0)

	vm.wdelay_us = 0;
	hc_wr(CLKCTL, TIMEOUT_MAX | CLK_INT_EN);
	STEP(1, rcg_set(RCG_50M));                      /* a fast MCLK for the core reset */
	delay(10);
	core_wr(MCI_POWER, core_rd(MCI_POWER) | 0x80);
	STEP(2, mmc_poll(COREREG(MCI_STATUS2), 1, 0, 10) || mmc_poll(COREREG(MCI_POWER), 0x80, 0, 10));
	core_wr(MCI_HC_MODE, 1);
	core_wr(MCI_HC_MODE, 1 | 1u << 13);
	pwrctl_ack();
	hc_wr(VENDOR_SPEC, (hc_rd(VENDOR_SPEC) & ~(VS_SELECT_IN | VS_MCLK_MASK)) |
	    VS_MCLK_DFLT | VS_IO_PAD_EN | VS_IO_PAD_18V);
	hc_wr(SIGEN, 0);                                /* polled */
	hc_wr(INTEN, 0x03ff0033u);                      /* cmd, xfer, rd/wr ready, all errors */
	hc_wr(HOSTCTL, PWR_18V_ON);
	delay(1000);
	pwrctl_ack();
	hc_wr(HOSTCTL2, HC2_18V);
	delay(1000);
	pwrctl_ack();
	STEP(3, set_clock(RCG_400K, 10));               /* 10 us = 4 card clocks after each write */
	delay(1000);

	STEP(4, mmc_cmd(0, 0, 0, 0, 0));
	delay(1000);
	end = deadline_ms(1000);
	for (;;) {
		STEP(5, mmc_cmd(1, OCR_SECTOR_18V, F_R3, 0, 0));
		if (vm.resp & 0x80000000u) {
			break;
		}
		STEP(6, mach_absolute_time() > end);
		delay(1000);
	}
	STEP(7, mmc_cmd(2, 0, F_R2, 0, 0));
	if (mmc_cmd(3, RCA << 16, F_R1, 0, 0)) {
		/* the controller can miss CMD3's response (seen in the shim); the card state decides */
		recover();
		STEP(8, mmc_state() != 3);
	}
	STEP(9, mmc_cmd(7, RCA << 16, F_R1B, 0, 0));
	STEP(10, switch_byte(183, 2));                  /* BUS_WIDTH: 8-bit SDR */
	hc_wr(HOSTCTL, PWR_18V_ON | HC_8BIT);
	STEP(11, switch_byte(185, 1));                  /* HS_TIMING: High Speed */
	hc_wr(HOSTCTL, PWR_18V_ON | HC_8BIT | HC_HISPEED);
	STEP(12, set_clock(RCG_50M, 0));
	STEP(13, mmc_state() != 4);
	return 0;
fail:
	mlog("VINCE MMC: init failed at step %d (status 0x%08x)\n", step, vm.err);
	recover();
	return EIO;
#undef STEP
}

/* n (1..MAX_SECTORS) sectors at lba into the bounce buffer */
static int
mmc_read_sectors(uint64_t lba, uint32_t n)
{
	if (lba > UINT32_MAX || n == 0 || n > MAX_SECTORS) {
		return EINVAL;
	}
	uint32_t xm = XM_READ | XM_BLKCNT | (n > 1 ? XM_MULTI | XM_ACMD12 : 0);
	int error = mmc_cmd(n > 1 ? 18 : 17, (uint32_t)lba, F_R1 | C_DATA, xm, n);
	if (!error && (vm.resp & R1_ERR)) {
		vm.err = vm.resp;
		error = EIO;
	}
	uint32_t *w = vince_mmc_bounce;
	for (uint32_t b = 0; !error && b < n; b++) {
		error = wait_int(INT_RDREADY, 1000);
		for (uint32_t i = 0; !error && i < SECTOR / 4; i++) {
			*w++ = hc_rd(BUF);
		}
	}
	if (!error) {
		error = wait_int(INT_XFER, 1000);
	}
	if (error) {
		mlog("VINCE MMC: read of %u sectors at 0x%llx failed (status 0x%08x)\n", n, lba, vm.err);
		recover();
	}
	return error;
}

static uint32_t
crc32_le(const uint8_t *p, size_t n)
{
	uint32_t c = 0xffffffffu;
	while (n--) {
		c ^= *p++;
		for (int k = 0; k < 8; k++) {
			c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
		}
	}
	return ~c;
}

static int
mmc_find_partition(const char *want)
{
	static uint8_t hdr[SECTOR];
	const uint8_t *b = (const uint8_t *)vince_mmc_bounce;
	int error = mmc_read_sectors(1, 1);
	if (error) {
		return error;
	}
	memcpy(hdr, b, SECTOR);
	if (memcmp(hdr, "EFI PART", 8) != 0) {
		mlog("VINCE MMC: no GPT\n");
		return ENXIO;
	}
	uint64_t entry_lba;
	uint32_t hsz, hcrc, nent, esz, ecrc;
	memcpy(&hsz, hdr + 12, 4);
	memcpy(&hcrc, hdr + 16, 4);
	memcpy(&entry_lba, hdr + 72, 8);
	memcpy(&nent, hdr + 80, 4);
	memcpy(&esz, hdr + 84, 4);
	memcpy(&ecrc, hdr + 88, 4);
	if (hsz < 92 || hsz > SECTOR || esz != 128 || nent == 0 || nent * esz > MAX_SECTORS * SECTOR) {
		mlog("VINCE MMC: unsupported GPT (header %u, entries %u x %u)\n", hsz, nent, esz);
		return ENXIO;
	}
	memset(hdr + 16, 0, 4);
	if (crc32_le(hdr, hsz) != hcrc) {
		mlog("VINCE MMC: GPT header CRC mismatch\n");
		return EIO;
	}
	error = mmc_read_sectors(entry_lba, (nent * esz + SECTOR - 1) / SECTOR);
	if (error) {
		return error;
	}
	if (crc32_le(b, nent * esz) != ecrc) {
		mlog("VINCE MMC: GPT entry CRC mismatch\n");
		return EIO;
	}
	for (uint32_t i = 0; i < nent; i++) {
		const uint8_t *e = b + i * esz;
		char name[37];
		unsigned c;
		for (c = 0; c < 36; c++) {
			uint16_t ch = (uint16_t)(e[56 + 2 * c] | e[57 + 2 * c] << 8);
			if (ch == 0 || ch > 0x7f) {
				break;
			}
			name[c] = (char)ch;
		}
		name[c] = 0;
		if (strcmp(name, want) != 0) {
			continue;
		}
		uint64_t first, last;
		memcpy(&first, e + 32, 8);
		memcpy(&last, e + 40, 8);
		if (last < first) {
			return ENXIO;
		}
		vm.first = first;
		vm.nsect = last - first + 1;
		return 0;
	}
	mlog("VINCE MMC: no GPT partition named \"%s\"\n", want);
	return ENOENT;
}

/* What the partition holds, for the log: the superblock magics at offset 1024 (HFS+, ext4, F2FS). */
static const char *
mmc_fs_signature(void)
{
	if (mmc_read_sectors(vm.first, 4)) {
		return "unreadable";
	}
	const uint8_t *p = (const uint8_t *)vince_mmc_bounce + 1024;
	if ((p[0] == 'H' && p[1] == '+') || (p[0] == 'H' && p[1] == 'X')) {
		return "HFS+";
	}
	if (p[0x38] == 0x53 && p[0x39] == 0xef) {
		return "ext4";
	}
	if (p[0] == 0x10 && p[1] == 0x20 && p[2] == 0xf5 && p[3] == 0xf2) {
		return "F2FS";
	}
	return "unknown";
}

/* ---- Punto de entrada del shim: escanea la eMMC y lista la tabla GPT (solo lectura). ---- */
int mmc_shim_scan(void) {
    vm.base = SDCC1_PHYS;
    vm.gcc = GCC_SDCC1_PHYS;
    vm.secsize = DEV_BSIZE;
    uint32_t lk_cmd = (hc_rd(XFER) >> 24) & 0x3f;
    int error = mmc_card_init();
    if (error) {
        mlog("init eMMC fallo: %d (ultimo CMD de LK: CMD%u)", error, lk_cmd);
        return error;
    }
    error = mmc_read_sectors(1, 1);
    if (error) { mlog("no se pudo leer el bloque 1 (GPT)"); return error; }
    static uint8_t hdr[SECTOR];
    const uint8_t *b = (const uint8_t *)vince_mmc_bounce;
    memcpy(hdr, b, SECTOR);
    if (memcmp(hdr, "EFI PART", 8) != 0) { mlog("sin GPT en el bloque 1"); return ENXIO; }
    uint64_t entry_lba; uint32_t nent, esz;
    memcpy(&entry_lba, hdr + 72, 8);
    memcpy(&nent, hdr + 80, 4);
    memcpy(&esz, hdr + 84, 4);
    mlog("GPT OK: %u particiones, entradas en LBA %llx", nent, (unsigned long long)entry_lba);
    error = mmc_read_sectors(entry_lba, (nent * esz + SECTOR - 1) / SECTOR);
    if (error) { mlog("no se pudieron leer las entradas"); return error; }
    char line[80]; unsigned lp = 0, shown = 0;
    for (uint32_t i = 0; i < nent; i++) {
        const uint8_t *e = b + i * esz;
        uint64_t first, last;
        memcpy(&first, e + 32, 8);
        memcpy(&last, e + 40, 8);
        if (last < first) continue;
        char name[37]; unsigned c;
        for (c = 0; c < 36; c++) {
            uint16_t ch = (uint16_t)(e[56 + 2 * c] | e[57 + 2 * c] << 8);
            if (ch == 0 || ch > 0x7f) break;
            name[c] = (char)ch;
        }
        name[c] = 0;
        if (strcmp(name, "userdata") == 0) {
            mlog("userdata: LBA %llx, %llu sectores = %llu bytes",
                 (unsigned long long)first, (unsigned long long)(last - first + 1),
                 (unsigned long long)((last - first + 1) * 512ull));
        }
        if (shown < 48) {
            for (unsigned k = 0; name[k] && lp < sizeof(line) - 12; k++) line[lp++] = name[k];
            line[lp++] = ' ';
            shown++;
            if (shown % 4 == 0 || i == nent - 1) { line[lp] = 0; mlog("%s", line); lp = 0; }
        }
    }
    return 0;
}

/* ---- Sonda de solo lectura de la SD (mmc1 = SDCC2) ---- */
#define SD_SDHCI_PHYS   0x07864900ul   /* registros SDHCI de la SD (reg del DTB) */
#define GCC_PHYS        0x01800000ul
#define GCC_SDCC2_RCG   0x43004ul      /* sdcc2_apps_clk_src: cmd_rcgr (gcc-msm8953.c) */
#define GCC_SDCC2_CBCR  0x43018ul      /* branch enable del SDCC2 (patrón del SDCC1: 0x42018) */

static inline uint32_t mmio_rd32(uintptr_t a) { return *(volatile uint32_t *)a; }
static inline void mmio_wr32(uintptr_t a, uint32_t v) { *(volatile uint32_t *)a = v; }
#define GCC_SDCC2_AHB_CBCR 0x4301cul  /* gcc_sdcc2_ahb_clk: halt_reg = enable_reg, bit0 = enable, bit31 = CLK_OFF */

/* Enciende el reloj de interfaz del SDCC2 (sólo si está apagado) y espera a que se confirme. */
static int sd_clock_on(void) {
    uint32_t v = mmio_rd32(GCC_PHYS + GCC_SDCC2_AHB_CBCR);
    mlog("GCC SDCC2 AHB CBCR antes 0x%08x (apagado=%u)", v, (v >> 31) & 1);
    if (!((v >> 31) & 1)) return 0;          /* ya está encendido */
    mmio_wr32(GCC_PHYS + GCC_SDCC2_AHB_CBCR, v | 1u);
    uint64_t end = mach_absolute_time() + (uint64_t)1000 * 192u / 10u * 1000u / 1000u; /* ~1 ms */
    while (mach_absolute_time() < end) {
        v = mmio_rd32(GCC_PHYS + GCC_SDCC2_AHB_CBCR);
        if (!((v >> 31) & 1)) break;
    }
    mlog("GCC SDCC2 AHB CBCR despues 0x%08x (apagado=%u)", v, (v >> 31) & 1);
    return ((v >> 31) & 1) ? -1 : 0;
}

/* Detección de tarjeta: GPIO 133 del TLMM (cd-gpios del árbol). Solo lectura.
 * Por patrón de msm8953: control en 0x1000*pin, entrada (IN) en 0x1000*pin + 0x4, bit0 = IN, bit9 = OE. */
#define TLMM_PHYS 0x01000000ul
#define CD_PIN    133u
static void sd_gpio_probe(void) {
    uintptr_t base = TLMM_PHYS + 0x1000ul * CD_PIN;
    uint32_t ctl = mmio_rd32(base);
    uint32_t io  = mmio_rd32(base + 0x4);
    mlog("GPIO133 CTL 0x%08x (salida=%u)", ctl, (ctl >> 9) & 1);
    mlog("GPIO133 IN=%u (io 0x%08x)", io & 1, io);
}

/* Lectura de solo lectura del reloj raíz del SDCC2 (rcg2): cmd, cfg, M, N, D. No escribe. */
static void sd_rcg_probe(void) {
    uint32_t cmd = mmio_rd32(GCC_PHYS + 0x43004ul);
    uint32_t cfg = mmio_rd32(GCC_PHYS + 0x43008ul);
    uint32_t m   = mmio_rd32(GCC_PHYS + 0x4300cul);
    uint32_t n   = mmio_rd32(GCC_PHYS + 0x43010ul);
    uint32_t d   = mmio_rd32(GCC_PHYS + 0x43014ul);
    mlog("SDCC2 RCG cmd=0x%08x cfg=0x%08x", cmd, cfg);
    mlog("SDCC2 RCG M=0x%x N=0x%x D=0x%x (raiz_off=%u)", m, n, d, (cmd >> 31) & 1);
}

/* Configura la raíz del reloj del SDCC2 a 400 kHz con los mismos valores que el driver del vince
 * (RCG_400K: cfg 0x2017, M=1, N=4; XO 19,2 MHz / 12 * 1/4). Usa su propia rcg_set con base SDCC2. */
static int sd_rcg_config(void) {
    uint32_t c0 = mmio_rd32(GCC_PHYS + 0x43004ul);
    mlog("RCG antes cmd=0x%08x (raiz %s)", c0, ((c0 >> 31) & 1) ? "apagada" : "encendida");
    vm.gcc = GCC_PHYS + 0x43000ul;          /* rcg_wr(i) escribe en vm.gcc + 4 + 4*i */
    /* Orden: cfg, M, N, D y luego CMD = UPDATE (bit0) | ROOT_EN (bit1). */
    rcg_wr(1, 0x2017u);
    rcg_wr(2, 1u);
    rcg_wr(3, ~(4u - 1u) & 0xffu);
    rcg_wr(4, ~4u & 0xffu);
    rcg_wr(0, 0x3u);
    int r = mmc_poll((volatile uint32_t *)(vm.gcc + 4), 1, 0, 10);
    uint32_t c1 = mmio_rd32(GCC_PHYS + 0x43004ul);
    uint32_t cf = mmio_rd32(GCC_PHYS + 0x43008ul);
    uint32_t m  = mmio_rd32(GCC_PHYS + 0x4300cul);
    uint32_t n  = mmio_rd32(GCC_PHYS + 0x43010ul);
    uint32_t d  = mmio_rd32(GCC_PHYS + 0x43014ul);
    mlog("RCG cmd=0x%08x cfg=0x%08x", c1, cf);
    mlog("RCG M=0x%x N=0x%x D=0x%x poll=%d", m, n, d, r);
    return r;
}

/* Comparación de solo lectura: reloj del SDCC1 (eMMC), que LK sí deja funcionando. */
static void emmc_rcg_probe(void) {
    uint32_t cmd = mmio_rd32(GCC_PHYS + 0x42004ul);
    uint32_t cfg = mmio_rd32(GCC_PHYS + 0x42008ul);
    uint32_t m   = mmio_rd32(GCC_PHYS + 0x4200cul);
    uint32_t n   = mmio_rd32(GCC_PHYS + 0x42010ul);
    uint32_t d   = mmio_rd32(GCC_PHYS + 0x42014ul);
    uint32_t cb  = mmio_rd32(GCC_PHYS + 0x42018ul);
    mlog("EMMC RCG cmd=0x%08x cfg=0x%08x M=0x%x N=0x%x D=0x%x", cmd, cfg, m, n, d);
    mlog("EMMC CBCR 0x%08x (apagado=%u)", cb, (cb >> 31) & 1);
}

/* Reloj de la tarjeta (apps) del SDCC2: branch en 0x43018, bit0 = enable, bit31 = CLK_OFF. */
#define GCC_SDCC2_APPS_CBCR 0x43018ul
static int sd_apps_clock_on(void) {
    uint32_t v = mmio_rd32(GCC_PHYS + GCC_SDCC2_APPS_CBCR);
    mlog("APPS CBCR antes 0x%08x (apagado=%u)", v, (v >> 31) & 1);
    if (!((v >> 31) & 1)) return 0;
    mmio_wr32(GCC_PHYS + GCC_SDCC2_APPS_CBCR, v | 1u);
    uint64_t end = mach_absolute_time() + 19200u;   /* ~1 ms */
    while (mach_absolute_time() < end) {
        v = mmio_rd32(GCC_PHYS + GCC_SDCC2_APPS_CBCR);
        if (!((v >> 31) & 1)) break;
    }
    mlog("APPS CBCR despues 0x%08x (apagado=%u)", v, (v >> 31) & 1);
    return ((v >> 31) & 1) ? -1 : 0;
}

/* ---- Identificación SD (sólo lectura): CMD0, CMD8, ACMD41, CMD2, CMD3, CMD7 y CMD17 del bloque 0. ---- */
#define SDR_ARG 0x08
#define SDR_XFER 0x0c
#define SDR_CMD 0x0e
#define SDR_RESP 0x10
#define SDR_BLKSZ 0x04
#define SDR_DATA 0x20
#define SDR_PSTATE 0x24
#define SDR_PWR 0x29
#define SDR_CLK 0x2c
#define SDR_SWRST 0x2f
#define SDR_INT 0x30
static inline uint16_t sd16(uint32_t o) { return *(volatile uint16_t *)(SD_SDHCI_PHYS + o); }
static inline void sd16w(uint32_t o, uint16_t v) { *(volatile uint16_t *)(SD_SDHCI_PHYS + o) = v; }
static inline uint8_t sd8(uint32_t o) { return *(volatile uint8_t *)(SD_SDHCI_PHYS + o); }
static inline void sd8w(uint32_t o, uint8_t v) { *(volatile uint8_t *)(SD_SDHCI_PHYS + o) = v; }
static inline void sd32w(uint32_t o, uint32_t v) { *(volatile uint32_t *)(SD_SDHCI_PHYS + o) = v; }
static inline uint32_t sd32(uint32_t o) { return *(volatile uint32_t *)(SD_SDHCI_PHYS + o); }

static int sd_timeout_wait(uint32_t o, uint32_t mask, uint32_t want, uint32_t ms) {
    uint64_t end = mach_absolute_time() + (uint64_t)ms * 19200u;
    while (mach_absolute_time() < end) {
        if ((sd32(o) & mask) == want) return 0;
    }
    return -1;
}

/* Envía un comando. flags: 0x00 sin respuesta, 0x02 R3, 0x1a R1/R6/R7, 0x09 R2, 0x1b R1b. Devuelve 0 o el estado. */
static uint32_t sd_cmd_status;
static int sd_cmd(uint8_t idx, uint32_t arg, uint16_t flags, uint16_t xfer, uint32_t *resp0) {
    uint64_t end = mach_absolute_time() + 10ull * 19200u;
    while (mach_absolute_time() < end) {
        if (!(sd32(0x24) & 0x3)) break;
    }
    sd16w(SDR_INT, 0xffff);
    sd32w(SDR_ARG, arg);
    if (xfer) sd16w(SDR_XFER, xfer);
    sd16w(SDR_CMD, (uint16_t)((idx << 8) | flags));
    end = mach_absolute_time() + 10ull * 19200u;
    uint16_t st = 0;
    while (mach_absolute_time() < end) {
        st = sd16(SDR_INT);
        if (st & 0x8001) break;
    }
    sd_cmd_status = st;
    if (resp0) *resp0 = sd32(SDR_RESP);
    sd16w(SDR_INT, st);
    if ((st & 0x8001) != 0x0001) return st ? st : 0xffff;
    return 0;
}

/* Preparación del núcleo MCI del SDCC2, como en mmc_card_init del vince:
 * reinicio del núcleo, paso a modo SDHCI, confirmación del bus y configuración del controlador. */
#define MCI_POWER   0x000
#define MCI_STATUS2 0x06c
#define MCI_HC_MODE 0x078
#define VS_MCLK_MASK    (3u << 8)
#define VS_MCLK_DFLT    (2u << 8)
#define VS_IO_PAD_EN    (1u << 15)
#define VS_SELECT_IN    (0xfu << 18)
static int sd_mci_init(void) {
    vm.base = 0x07864000ul;
    core_wr(MCI_POWER, core_rd(MCI_POWER) | 0x80u);
    uint64_t end = mach_absolute_time() + 10ull * 19200u;
    while (mach_absolute_time() < end && ((core_rd(MCI_POWER) & 0x80u) || (core_rd(MCI_STATUS2) & 1u))) { }
    if (core_rd(MCI_POWER) & 0x80u) { mlog("SD MCI no sale del reset"); return -1; }
    core_wr(MCI_HC_MODE, 1u);
    core_wr(MCI_HC_MODE, 1u | (1u << 13));
    pwrctl_ack();
    hc_wr(VENDOR_SPEC, (hc_rd(VENDOR_SPEC) & ~(VS_SELECT_IN | VS_MCLK_MASK)) | VS_MCLK_DFLT | VS_IO_PAD_EN);
    hc_wr(SIGEN, 0);
    hc_wr(INTEN, 0x03ff0033u);
    pwrctl_ack();
    mlog("SD MCI listo: hc_mode=0x%x", core_rd(MCI_HC_MODE));
    return 0;
}

static int sd_identify(void) {
    uint32_t r = 0;
    if (sd_mci_init()) return -1;
    /* Pines SDC2 (TLMM 0x109000, del driver pinctrl-msm8953): fuerza 16 mA y pull-up en CMD/DATA.
     * Solo se modifican los bits de fuerza (drv) y resistencia (pull) de cada pin. */
    {
        uintptr_t tlmm_sdc2 = TLMM_PHYS + 0x109000ul;
        uint32_t old = mmio_rd32(tlmm_sdc2);
        uint32_t v = old;
        v &= ~((3u << 14) | (7u << 6));  v |= (0u << 14) | (7u << 6);    /* clk: sin pull, 16 mA */
        v &= ~((3u << 11) | (7u << 3));  v |= (3u << 11) | (7u << 3);    /* cmd: pull-up, 16 mA */
        v &= ~((3u << 9)  | (7u << 0));  v |= (3u << 9)  | (7u << 0);    /* data: pull-up, 16 mA */
        mmio_wr32(tlmm_sdc2, v);
        mlog("SDC2 pines antes=0x%08x despues=0x%08x", old, mmio_rd32(tlmm_sdc2));
    }
    /* 1) reset del controlador y reloj a 400 kHz (raíz ya configurada) */
    sd8w(SDR_SWRST, 0x01);
    {   /* el reset por software es un registro de 8 bits (0x2F): se lee como byte, nunca como palabra de 32 */
        uint64_t rend = mach_absolute_time() + 10ull * 19200u;
        while (mach_absolute_time() < rend && (sd8(SDR_SWRST) & 0x01)) { }
        if (sd8(SDR_SWRST) & 0x01) { mlog("SD reset no termina"); return -1; }
    }
    /* Secuencia de Linux: apagar el bus, esperar, encender a 3,3 V y confirmar la petición PWRCTL. */
    sd8w(SDR_PWR, 0x00);
    delay(10000);                            /* 10 ms con el bus apagado */
    sd8w(SDR_PWR, 0x0f);                     /* 3,3 V */
    delay(10000);
    /* El núcleo MSM pide el bus con PWRCTL y no deja pasar comandos hasta que se confirma (como en el vince). */
    vm.base = 0x07864000ul;                  /* núcleo SDCC2 (reg del árbol); los registros SDHCI están en +0x900 */
    pwrctl_ack();
    mlog("SD PWRCTL st=0x%08x pstate=0x%08x", core_rd(PWRCTL_STATUS), sd32(0x24));
    mlog("SD PWR leido=0x%02x (0x0f = encendido)", sd8(SDR_PWR));
    /* Secuencia SDHCI: timeout + reloj interno, esperar "estable" (bit1, sólo lectura), y luego el reloj de tarjeta.
     * El bit de estable NO se puede escribir: hay que mantener bit0 y añadir bit2 sin borrarlo. */
    sd32w(SDR_CLK, (0xeu << 16) | 0x0001u);
    if (sd_timeout_wait(SDR_CLK, 0x0002, 0x0002, 10)) { mlog("reloj interno no estable: CLK=0x%04x", sd16(SDR_CLK)); return -1; }
    sd16w(SDR_CLK, 0x0007);                   /* reloj interno + reloj de tarjeta (bit0 y bit2) */
    mlog("SD CLK tras habilitar=0x%04x (estable=%u)", sd16(SDR_CLK), (sd16(SDR_CLK) >> 1) & 1);

    mlog("SD CLK=0x%04x PSTATE=0x%08x INT=0x%08x", sd16(SDR_CLK), sd32(0x24), sd32(SDR_INT));
    mlog("SD CAPS=0x%08x HOST=0x%02x PWR=0x%02x", sd32(0x40), sd8(0x28), sd8(SDR_PWR));
    /* 2) CMD0 */
    int e = sd_cmd(0, 0, 0x00, 0, NULL);
    mlog("SD CMD0 %s (st=0x%04x)", e ? "FALLO" : "ok", sd_cmd_status);
    if (e) {   /* diagnóstico: estado completo del comando, sin escribir nada */
        mlog("INT=0x%08x INTEN=0x%08x ERR=0x%04x", sd32(SDR_INT), sd32(0x34), sd16(0x32));
        mlog("CMD=0x%04x ARG=0x%08x PSTATE=0x%08x", sd16(SDR_CMD), sd32(SDR_ARG), sd32(0x24));
    }
    if (e) return -1;

    /* 3) CMD8: ¿tarjeta SD v2? */
    e = sd_cmd(8, 0x1aa, 0x1a, 0, &r);
    mlog("SD CMD8 %s resp=0x%08x (esperado 0x000001aa)", e ? "FALLO" : "ok", r);
    int v2 = (!e && (r & 0xfff) == 0x1aa);

    /* 4) ACMD41 hasta que la tarjeta esté lista */
    uint32_t ocr = 0;
    for (int i = 0; i < 1000; i++) {
        if (sd_cmd(55, 0, 0x1a, 0, NULL)) break;
        e = sd_cmd(41, v2 ? 0x40ff8000u : 0x00ff8000u, 0x02, 0, &ocr);
        if (e) break;
        if (ocr & 0x80000000u) break;
        delay(1000);
    }
    mlog("SD ACMD41 ocr=0x%08x (lista=%u ccs=%u)", ocr, (ocr >> 31) & 1, (ocr >> 30) & 1);
    if (!((ocr >> 31) & 1)) { mlog("la tarjeta no esta lista: revisar alimentacion L11"); return -1; }

    /* 5) CMD2, CMD3 (RCA), CMD7 (seleccionar) */
    sd_cmd(2, 0, 0x09, 0, NULL);
    uint32_t rca_r = 0;
    e = sd_cmd(3, 0, 0x1a, 0, &rca_r);
    uint16_t rca = (uint16_t)(rca_r >> 16);
    mlog("SD CMD3 %s rca=0x%04x", e ? "FALLO" : "ok", rca);
    if (e) return -1;
    e = sd_cmd(7, (uint32_t)rca << 16, 0x1b, 0, NULL);
    mlog("SD CMD7 %s", e ? "FALLO" : "ok");
    if (e) return -1;

    /* 6) CMD17: leer el bloque 0 (512 bytes, PIO) */
    sd16w(SDR_BLKSZ, 0x0200);
    e = sd_cmd(17, 0, 0x3a, 0x0010, NULL);
    if (e) { mlog("CMD17 FALLO st=0x%04x", sd_cmd_status); return -1; }
    uint64_t end = mach_absolute_time() + 10ull * 19200u;
    while (mach_absolute_time() < end && !(sd16(SDR_INT) & 0x20)) { }
    uint8_t blk[512];
    for (int i = 0; i < 128; i++) {
        uint32_t w = sd32(SDR_DATA);
        blk[i*4+0] = (uint8_t)w; blk[i*4+1] = (uint8_t)(w >> 8);
        blk[i*4+2] = (uint8_t)(w >> 16); blk[i*4+3] = (uint8_t)(w >> 24);
    }
    sd16w(SDR_INT, 0xffff);
    char oem[9];
    for (int i = 0; i < 8; i++) oem[i] = (blk[3 + i] >= 32 && blk[3 + i] < 127) ? (char)blk[3 + i] : '.';
    oem[8] = 0;
    mlog("SD BLK0 OEM=%s firma=0x%02x%02x", oem, blk[510], blk[511]);
    return 0;
}

int sd_probe(void) {
    sd_gpio_probe();
    sd_rcg_probe();
    if (sd_rcg_config()) { mlog("la raiz del reloj no confirmo la actualizacion"); return -1; }
    sd_apps_clock_on();
    if (sd_clock_on()) { mlog("el reloj del SDCC2 no se ha encendido: NO leo la SD"); return -1; }
    sd_identify();
    return 0;
    uint32_t ps   = mmio_rd32(SD_SDHCI_PHYS + 0x24);   /* PRESENT_STATE */
    uint32_t ctl  = mmio_rd32(SD_SDHCI_PHYS + 0x2c);   /* CLOCK_CONTROL (16 bits bajos) */
    uint32_t cap  = mmio_rd32(SD_SDHCI_PHYS + 0x40);   /* CAPABILITIES */
    uint32_t rcg  = mmio_rd32(GCC_PHYS + GCC_SDCC2_RCG);
    uint32_t cbcr = mmio_rd32(GCC_PHYS + GCC_SDCC2_CBCR);
    mlog("SD PRESENT 0x%08x (insertada=%u, estable=%u, pin=%u)", ps,
         (ps >> 16) & 1, (ps >> 17) & 1, (ps >> 18) & 1);
    mlog("SD CLOCK_CONTROL 0x%08x  CAPS 0x%08x", ctl & 0xffff, cap);
    mlog("GCC SDCC2 RCG 0x%08x  CBCR 0x%08x (bit31=apagado: %u)", rcg, cbcr, (cbcr >> 31) & 1);
    return 0;
}
