/*
 * libtilemcore - Graphing calculator emulation library
 *
 * Copyright (C) 2009-2016 Benjamin Moody
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 */

/* Flag magic:

   S (the Sign flag) is set when the result is negative (i.e., copied
   from the MS bit.)

   Z (the Zero flag) is set when the result is zero.

   Y (undocumented flag) is usually copied from the result.

   H (the Half-carry flag) is set when a carry occurs from LS to MS
   nibble.  Since bit 4 of the result is equal to (bit 4 of first arg)
   xor (bit 4 of second arg) xor H, we can infer that H equals (bit 4
   of first arg) xor (bit 4 of second arg) xor (bit 4 of result).

   X (undocumented flag) is copied from the same place as is Y.

   P (parity flag) is set -- by logic operations -- when the result
   has even parity; i.e., it's the xor of all the bits of the result
   and 1.

   V (overflow flag, which is another name for the parity flag) is set
   -- by arithmetic operations -- when the result overflows.  This can
   happen when a positive plus a positive is negative, or a negative
   plus a negative is positive, or a positive minus a negative is
   negative, or a negative minus a positive is positive.

   (In 8 bits, a positive plus a negative is at least 0 + -128 = -128
   and at most 127 + -1 + 1 = 127.  A positive minus a positive is at
   least 0 - 127 - 1 = -128 and at most 127 - 0 = 127.  A negative
   minus a negative is at least -128 - -1 - 1 = -128 and at most -1 -
   -128 = 127.  Thus these cases can never overflow.)

   When adding, then, overflow occurs if the args are equal in the
   MS bit and the result differs in the MS bit.

   When subtracting, overflow occurs if the args differ in the MS
   bit and the result differs from the first arg in the MS bit.

   N (subtract flag) is set or cleared depending on the operation.

   C (carry flag) is the leftover bit after an addition or
   subtraction.
*/

#define FLAG_XY (FLAG_X | FLAG_Y)
#define FLAG_XYC (FLAG_X | FLAG_Y | FLAG_C)
#define FLAG_SXY (FLAG_S | FLAG_X | FLAG_Y)
#define FLAG_SXYC (FLAG_S | FLAG_X | FLAG_Y | FLAG_C)
#define FLAG_ZP (FLAG_Z | FLAG_P)
#define FLAG_SZPC (FLAG_S | FLAG_Z | FLAG_P | FLAG_C)

#define lsb(xxx) ((xxx) & 0xff)

#define UNDOCUMENTED(clks)						\
	if (calc->z80.emuflags & TILEM_Z80_BREAK_UNDOCUMENTED)		\
		tilem_z80_stop(calc, TILEM_STOP_UNDOCUMENTED_INST);	\
	if (calc->z80.emuflags & TILEM_Z80_SKIP_UNDOCUMENTED) {		\
		delay(clks);						\
		break;							\
	}								\
	if (calc->z80.emuflags & TILEM_Z80_RESET_UNDOCUMENTED) {	\
		tilem_warning(calc, _("Invalid opcode %x"), op); \
		tilem_z80_exception(calc, TILEM_EXC_INSTRUCTION);	\
		break;							\
	}

#define add8(dst, src) do {						\
		word arg1 = (dst);					\
		word arg2 = (src);					\
		word res = arg1 + arg2;					\
		byte resb = res;					\
		F = ((res & FLAG_SXY)			/* S/X/Y */	\
		     | (resb ? 0 : FLAG_Z)		/* Z */		\
		     | ((arg1 ^ arg2 ^ res) & FLAG_H)	/* H */		\
		     | (((arg1 ^ ~arg2) & (arg1 ^ res) & 0x80) >> 5)	\
		     | (res >> 8));			/* C */		\
		(dst) = resb;						\
	} while (0)

#define adc8(dst, src) do {						\
		word arg1 = (dst);					\
		word arg2 = (src);					\
		word res = arg1 + arg2 + (F & 1);			\
		byte resb = res;					\
		F = ((res & FLAG_SXY)			/* S/X/Y */	\
		     | (resb ? 0 : FLAG_Z)		/* Z */		\
		     | ((arg1 ^ arg2 ^ res) & FLAG_H)	/* H */		\
		     | (((arg1 ^ ~arg2) & (arg1 ^ res) & 0x80) >> 5)	\
		     | (res >> 8));			/* C */		\
		(dst) = resb;						\
	} while (0)

#define sub8(dst, src) do {						\
		word arg1 = (dst);					\
		word arg2 = (src);					\
		word res = arg1 - arg2;					\
		byte resb = res;					\
		F = ((res & FLAG_SXY)			/* S/X/Y */	\
		     | (resb ? 0 : FLAG_Z)		/* Z */		\
		     | ((arg1 ^ arg2 ^ res) & FLAG_H)	/* H */		\
		     | (((arg1 ^ arg2) & (arg1 ^ res) & 0x80) >> 5)	\
		     | (FLAG_N)				/* N */		\
		     | ((res >> 8) & 1));		/* C */		\
		(dst) = resb;						\
	} while (0)

#define sbc8(dst, src) do {						\
		word arg1 = (dst);					\
		word arg2 = (src);					\
		word res = arg1 - arg2 - (F & 1);			\
		byte resb = res;					\
		F = ((res & FLAG_SXY)			/* S/X/Y */	\
		     | (resb ? 0 : FLAG_Z)		/* Z */		\
		     | ((arg1 ^ arg2 ^ res) & FLAG_H)	/* H */		\
		     | (((arg1 ^ arg2) & (arg1 ^ res) & 0x80) >> 5)	\
		     | (FLAG_N)				/* N */		\
		     | ((res >> 8) & 1));		/* C */		\
		(dst) = resb;						\
	} while (0)

#define and(dst, src) do {						\
		byte arg1 = (dst);					\
		byte arg2 = (src);					\
		byte res = (arg1 & arg2);				\
		F = ((res & FLAG_SXY)			/* S/X/Y */	\
		     | (res ? 0 : FLAG_Z)		/* Z */		\
		     | (FLAG_H)				/* H */		\
		     | parity_table[res]);		/* P */		\
		(dst) = res;						\
	} while (0)

#define xor(dst, src) do {						\
		byte arg1 = (dst);					\
		byte arg2 = (src);					\
		byte res = (arg1 ^ arg2);				\
		F = ((res & FLAG_SXY)			/* S/X/Y */	\
		     | (res ? 0 : FLAG_Z)		/* Z */		\
		     | parity_table[res]);		/* P */		\
		(dst) = res;						\
	} while (0)

#define or(dst, src) do {						\
		byte arg1 = (dst);					\
		byte arg2 = (src);					\
		byte res = (arg1 | arg2);				\
		F = ((res & FLAG_SXY)			/* S/X/Y */	\
		     | (res ? 0 : FLAG_Z)		/* Z */		\
		     | parity_table[res]);		/* P */		\
		(dst) = res;						\
	} while (0)

#define cp(dst, src) do {						\
		word arg1 = (dst);					\
		word arg2 = (src);					\
		word res = arg1 - arg2;					\
		byte resb = res;					\
		F = ((res & FLAG_SXY)	                /* S/X/Y */	\
		     | (resb ? 0 : FLAG_Z)		/* Z */	\
		     | ((arg1 ^ arg2 ^ res) & FLAG_H)	/* H */		\
		     | (((arg1 ^ arg2) & (arg1 ^ res) & 0x80) >> 5)	\
		     | (FLAG_N)		                /* N */		\
		     | ((res >> 8) & 1));		/* C */		\
	} while (0)


#define add16(dst, src) do {						\
		dword arg1 = (word) (dst); 				\
		dword arg2 = (word) (src); 				\
		dword res = arg1 + arg2;				\
		F = ((F & (FLAG_S | FLAG_Z | FLAG_P))			\
		     | ((res >> 8) & FLAG_XY)		     /* X/Y */	\
		     | (((arg1 ^ arg2 ^ res) >> 8) & FLAG_H) /* H */	\
		     | (res >> 16));		   	     /* C */	\
		(dst) = res;						\
	} while (0)

#define adc16(dst, src) do {						\
		dword arg1 = (word) (dst);				\
		dword arg2 = (word) (src);				\
		dword res = arg1 + arg2 + (F & 1);			\
		word resw = res;					\
		F = (((res >> 8) & FLAG_SXY)		     /* S/X/Y */\
		     | (resw ? 0 : FLAG_Z)		     /* Z */	\
		     | (((arg1 ^ arg2 ^ res) >> 8) & FLAG_H) /* H */	\
		     | (((arg1 ^ ~arg2) & (arg1 ^ res) & 0x8000) >> 11) \
		     | (res >> 16));		     	     /* C */	\
		(dst) = resw;						\
	} while (0)

#define sbc16(dst, src) do {						\
		dword arg1 = (word) (dst);				\
		dword arg2 = (word) (src);				\
		dword res = arg1 - arg2 - (F & 1);			\
		word resw = res;					\
		F = (((res >> 8) & FLAG_SXY)		     /* S/X/Y */\
		     | (resw ? 0 : FLAG_Z)		     /* Z */	\
		     | (((arg1 ^ arg2 ^ res) >> 8) & FLAG_H) /* H */	\
		     | (((arg1 ^ arg2) & (arg1 ^ res) & 0x8000) >> 11)	\
		     | (FLAG_N)				     /* N */	\
		     | ((res >> 16) & 1));		     /* C */	\
		(dst) = resw;						\
	} while (0)


#define inc(reg) do {						\
		byte arg = (reg);				\
		byte res = arg + 1;				\
		F = ((res & FLAG_SXY)	       	    /* S/X/Y */	\
		     | (res ? 0 : FLAG_Z)	    /* Z */	\
		     | ((arg ^ res) & FLAG_H)	    /* H */	\
		     | (res == 0x80 ? FLAG_V : 0)   /* V */	\
		     | (F & FLAG_C));				\
		(reg) = res;					\
	} while (0)

#define dec(reg) do {						\
		byte arg = (reg);				\
		byte res = arg - 1;				\
		F = ((res & FLAG_SXY)	       	    /* S/X/Y */	\
		     | (res ? 0 : FLAG_Z)	    /* Z */	\
		     | ((arg ^ res) & FLAG_H)	    /* H */	\
		     | (res == 0x7f ? FLAG_V : 0)   /* V */	\
		     | (FLAG_N)			    /* N */	\
		     | (F & FLAG_C));				\
		(reg) = res;					\
	} while (0)


#define cpl(reg) do {						\
		byte arg = (reg);				\
		byte res = ~arg;				\
		F = ((res & FLAG_XY)		/* X/Y */	\
		     | FLAG_H | FLAG_N		/* H/N */	\
		     | (F & FLAG_SZPC));			\
		(reg) = res;					\
	} while (0)

#define neg(reg) do {						\
		byte arg = (reg);				\
		byte res = -arg;				\
		F = ((res & FLAG_SXY)		    /* S/X/Y */	\
		     | (res ? 0 : FLAG_Z)	    /* Z */	\
		     | ((arg ^ res) & FLAG_H)	    /* H */	\
		     | (arg == 0x80 ? FLAG_V : 0)   /* V */	\
		     | (FLAG_N)			    /* N */	\
		     | (!!arg));		    /* C */	\
		(reg) = res;					\
	} while (0)

#define daa do {							\
		word idx = (((F & 0x10) << 6) | ((F & 0x03) << 8) | A);	\
		AF = daa_table[idx];					\
	} while (0)


#define rlca do {							\
		byte arg = A;						\
		byte res = ((arg << 1) | (arg >> 7));			\
		F = ((F & (FLAG_S | FLAG_Z | FLAG_P))			\
		     | (res & FLAG_XYC));	          /* X/Y/C */	\
		A = res;						\
	} while (0)

#define rrca do {						\
		byte arg = A;					\
		byte res = ((arg >> 1) | (arg << 7));		\
		F = ((F & (FLAG_S | FLAG_Z | FLAG_P))		\
		     | (res & FLAG_XY)	          /* X/Y */	\
		     | (arg & FLAG_C));		  /* C */	\
		A = res;					\
	} while (0)
    
#define rla do {						\
		byte arg = A;					\
		byte res = ((arg << 1) | (F & 1));		\
		F = ((F & (FLAG_S | FLAG_Z | FLAG_P))		\
		     | (res & FLAG_XY)	          /* X/Y */	\
		     | (arg >> 7));		  /* C */	\
		A = res;					\
	} while (0)

#define rra do {						\
		byte arg = A;					\
		byte res = ((arg >> 1) | (F << 7));		\
		F = ((F & (FLAG_S | FLAG_Z | FLAG_P))		\
		     | (res & FLAG_XY)		  /* X/Y */	\
		     | (arg & FLAG_C));		  /* C */	\
		A = res;					\
	} while (0)

#define rlc(reg) do {						\
		byte arg = (reg);				\
		byte res = ((arg << 1) | (arg >> 7));		\
		F = ((res & FLAG_SXYC)	          /* S/X/Y/C */ \
		     | (res ? 0 : FLAG_Z)	  /* Z */       \
		     | parity_table[res]);	  /* P */	\
		(reg) = res;					\
	} while (0)

#define rrc(reg) do {						\
		byte arg = (reg);				\
		byte res = ((arg >> 1) | (arg << 7));		\
		F = ((res & FLAG_SXY)	          /* S/X/Y */	\
		     | (res ? 0 : FLAG_Z)	  /* Z */	\
		     | parity_table[res]	  /* P */	\
		     | (arg & FLAG_C));		  /* C */	\
		(reg) = res;					\
	} while (0)

#define rl(reg) do {						\
		byte arg = (reg);				\
		byte res = ((arg << 1) | (F & 1));		\
		F = ((res & FLAG_SXY)	          /* S/X/Y */	\
		     | (res ? 0 : FLAG_Z)	  /* Z */	\
		     | parity_table[res]	  /* P */	\
		     | (arg >> 7));		  /* C */	\
		(reg) = res;					\
	} while (0)

#define rr(reg) do {						\
		byte arg = (reg);				\
		byte res = ((arg >> 1) | (F << 7));		\
		F = ((res & FLAG_SXY)	          /* S/X/Y */	\
		     | (res ? 0 : FLAG_Z)	  /* Z */	\
		     | parity_table[res]	  /* P */	\
		     | (arg & FLAG_C));		  /* C */	\
		(reg) = res;					\
	} while (0)

#define sla(reg) do {						\
		byte arg = (reg);				\
		byte res = (arg << 1);				\
		F = ((res & FLAG_SXY)	          /* S/X/Y */	\
		     | (res ? 0 : FLAG_Z)	  /* Z */	\
		     | parity_table[res]	  /* P */	\
		     | (arg >> 7));		  /* C */	\
		(reg) = res;					\
	} while (0)

#define sra(reg) do {						\
		byte arg = (reg);				\
		byte res = ((signed char) arg >> 1);		\
		F = ((res & FLAG_SXY)	          /* S/X/Y */	\
		     | (res ? 0 : FLAG_Z)	  /* Z */	\
		     | parity_table[res]	  /* P */	\
		     | (arg & FLAG_C));		  /* C */	\
		(reg) = res;					\
	} while (0)

#define slia(reg) do {						\
		byte arg = (reg);				\
		byte res = ((arg << 1) | 1);			\
		F = ((res & FLAG_SXY)	          /* S/X/Y */	\
		     | (res ? 0 : FLAG_Z)	  /* Z */	\
		     | parity_table[res]	  /* P */	\
		     | (arg >> 7));		  /* C */	\
		(reg) = res;					\
	} while (0)

#define srl(reg) do {						\
		byte arg = (reg);				\
		byte res = (arg >> 1);				\
		F = ((res & FLAG_SXY)	          /* S/X/Y */	\
		     | (res ? 0 : FLAG_Z)	  /* Z */	\
		     | parity_table[res]	  /* P */	\
		     | (arg & FLAG_C));		  /* C */	\
		(reg) = res;					\
	} while (0)

#define rld do {						\
		byte arg1 = readb(HL);				\
		byte arg2 = A;					\
		byte res = ((arg2 & 0xf0) | (arg1 >> 4));	\
		WZ = HL + 1;					\
		writeb(HL, (arg1 << 4) | (arg2 & 0xf));		\
		F = ((res & FLAG_SXY)		/* S/X/Y */	\
		     | (res ? 0 : FLAG_Z)	/* Z */		\
		     | parity_table[res]	/* P */		\
		     | (F & FLAG_C));				\
		A = res;					\
	} while (0)

#define rrd do {						\
		byte arg1 = readb(HL);				\
		byte arg2 = A;					\
		byte res = ((arg2 & 0xf0) | (arg1 & 0x0f));	\
		WZ = HL + 1;					\
		writeb(HL, (arg1 >> 4) | (arg2 << 4));		\
		F = ((res & FLAG_SXY)		/* S/X/Y */	\
		     | (res ? 0 : FLAG_Z)	/* Z */		\
		     | parity_table[res]       	/* P */		\
		     | (F & FLAG_C));				\
		A = res;					\
	} while (0)

#define push(reg) do {				\
		SP -= 2;			\
		writew(SP, (reg));		\
	} while (0)

#define pop(reg) do {				\
		(reg) = readw(SP);		\
		SP += 2;			\
	} while (0)

#define ex(aaa, bbb) do {		\
		tmp2 = (aaa);		\
		(aaa) = (bbb);		\
		(bbb) = tmp2;		\
	} while (0)

#define in(reg) do {						\
		byte res = input(WZ);				\
		F = ((res & FLAG_SXY)       /* S/X/Y */		\
		     | (res ? 0 : FLAG_Z)   /* Z */		\
		     | parity_table[res]    /* P */		\
		     | (F & FLAG_C));				\
		(reg) = res;					\
	} while (0)


#define ld_a_ir(reg) do {					\
		byte res = (reg);				\
		A = res;					\
		F = ((res & FLAG_SXY)		/* S/X/Y */	\
		     | (res ? 0 : FLAG_Z)	/* Z */		\
		     | (IFF2 ? FLAG_P : 0)	/* P */		\
		     | (F & FLAG_C));				\
	} while (0)

#define ldi do {						\
		byte res = readb(HL++);				\
		writeb(DE++, res);				\
		BC--;						\
		F = ((F & (FLAG_S | FLAG_Z | FLAG_C))		\
		     | ((res << 4) & FLAG_Y)	/* Y */		\
		     | (res & FLAG_X)		/* X */		\
		     | (BCw ? FLAG_P : 0));	/* P */	\
	} while (0)

#define ldd do {						\
		byte res = readb(HL--);				\
		writeb(DE--, res);				\
		BC--;						\
		F = ((F & (FLAG_S | FLAG_Z | FLAG_C))		\
		     | ((res << 4) & FLAG_Y) 	/* Y */		\
		     | (res & FLAG_X)        	/* X */		\
		     | (BCw ? FLAG_P : 0));	/* P */		\
	} while (0)

/* FIXME: CPI and CPD obviously use WZ, but I haven't yet figured out
   precisely how. */

#define cpi do {						\
		word arg1 = A;					\
		word arg2 = readb(HL++);			\
		word res = arg1 - arg2;				\
		byte hc = ((arg1 ^ arg2 ^ res) & 0x10);		\
		byte res2 = res - (hc >> 4);			\
		BC--;						\
		F = ((res & FLAG_S)		   /* S */	\
		     | (lsb(res) ? 0 : FLAG_Z)	   /* Z */	\
		     | ((res2 << 4) & FLAG_Y)	   /* Y */	\
		     | hc			   /* H */	\
		     | (res2 & FLAG_X)		   /* X */	\
		     | (BCw ? FLAG_P : 0)	   /* P */	\
		     | FLAG_N			   /* N */	\
		     | (F & FLAG_C));				\
	} while (0)

#define cpd do {						\
		word arg1 = A;					\
		word arg2 = readb(HL--);			\
		word res = arg1 - arg2;				\
		byte hc = ((arg1 ^ arg2 ^ res) & 0x10);		\
		byte res2 = res - (hc >> 4);			\
		BC--;						\
		F = ((res & FLAG_S)		   /* S */	\
		     | (lsb(res) ? 0 : FLAG_Z)	   /* Z */	\
		     | ((res2 << 4) & FLAG_Y)	   /* Y */	\
		     | hc			   /* H */	\
		     | (res2 & FLAG_X)		   /* X */	\
		     | (BCw ? FLAG_P : 0)	   /* P */	\
		     | FLAG_N			   /* N */	\
		     | (F & FLAG_C));				\
	} while (0)

#define ini do {							\
		byte value, count;					\
		word k;							\
		value = input(BC);					\
		writeb(HL++, value);					\
		WZ = BC + 1;						\
		k = Z + value;						\
		count = --B;						\
		F = ((count & FLAG_SXY)			/* S/X/Y */	\
		     | (count ? 0 : FLAG_Z)		/* Z */		\
		     | ((k >> 8) ? 0x11 : 0)		/* H/C */	\
		     | parity_table[(k & 7) ^ count]	/* P */		\
		     | ((value >> 6) & FLAG_N));	/* N */		\
	} while (0)

#define ind do {							\
		byte value, count;					\
		word k;							\
		value = input(BC);					\
		writeb(HL--, value);					\
		WZ = BC - 1;						\
		k = Z + value;						\
		count = --B;						\
		F = ((count & FLAG_SXY)	  		/* S/X/Y */	\
		     | (count ? 0 : FLAG_Z)		/* Z */		\
		     | ((k >> 8) ? 0x11 : 0)    	/* H/C */	\
		     | parity_table[(k & 7) ^ count]	/* P */		\
		     | ((value >> 6) & FLAG_N)); 	/* N */		\
	} while (0)

#define outi do {							\
		byte value, count;					\
		word k;							\
		value = readb(HL++);					\
		count = --B;						\
		output(BC, value);					\
		k = L + value;						\
		WZ = BC + 1;						\
		F = ((count & FLAG_SXY)			/* S/X/Y */	\
		     | (count ? 0 : FLAG_Z)		/* Z */		\
		     | ((k >> 8) ? 0x11 : 0)	  	/* H/C */	\
		     | parity_table[(k & 7) ^ count]	/* P */		\
		     | ((value >> 6) & FLAG_N));	/* N */		\
	} while (0)

#define outd do {							\
		byte value, count;					\
		word k;							\
		value = readb(HL--);					\
		count = --B;						\
		output(BC, value);					\
		k = L + value;						\
		WZ = BC - 1;						\
		F = ((count & FLAG_SXY)			/* S/X/Y */	\
		     | (count ? 0 : FLAG_Z)		/* Z */		\
		     | ((k >> 8) ? 0x11 : 0)		/* H/C */	\
		     | parity_table[(k & 7) ^ count]	/* P */		\
		     | ((value >> 6) & FLAG_N));	/* N */		\
	} while (0)

/* Table giving parity flag values.  (Also known as the Thue-Morse
   sequence.) */
static const byte parity_table[256] = {
	4,0,0,4,0,4,4,0, 0,4,4,0,4,0,0,4, 0,4,4,0,4,0,0,4, 4,0,0,4,0,4,4,0,
	0,4,4,0,4,0,0,4, 4,0,0,4,0,4,4,0, 4,0,0,4,0,4,4,0, 0,4,4,0,4,0,0,4,
	0,4,4,0,4,0,0,4, 4,0,0,4,0,4,4,0, 4,0,0,4,0,4,4,0, 0,4,4,0,4,0,0,4,
	4,0,0,4,0,4,4,0, 0,4,4,0,4,0,0,4, 0,4,4,0,4,0,0,4, 4,0,0,4,0,4,4,0,

	0,4,4,0,4,0,0,4, 4,0,0,4,0,4,4,0, 4,0,0,4,0,4,4,0, 0,4,4,0,4,0,0,4,
	4,0,0,4,0,4,4,0, 0,4,4,0,4,0,0,4, 0,4,4,0,4,0,0,4, 4,0,0,4,0,4,4,0,
	4,0,0,4,0,4,4,0, 0,4,4,0,4,0,0,4, 0,4,4,0,4,0,0,4, 4,0,0,4,0,4,4,0,
	0,4,4,0,4,0,0,4, 4,0,0,4,0,4,4,0, 4,0,0,4,0,4,4,0, 0,4,4,0,4,0,0,4,
};

/* Table giving AF values following DAA */
static const word daa_table[2048] = {
	0x0044, 0x0100, 0x0200, 0x0304, 0x0400, 0x0504, 0x0604, 0x0700,
	0x0808, 0x090c, 0x1010, 0x1114, 0x1214, 0x1310, 0x1414, 0x1510,
	0x1000, 0x1104, 0x1204, 0x1300, 0x1404, 0x1500, 0x1600, 0x1704,
	0x180c, 0x1908, 0x2030, 0x2134, 0x2234, 0x2330, 0x2434, 0x2530,
	0x2020, 0x2124, 0x2224, 0x2320, 0x2424, 0x2520, 0x2620, 0x2724,
	0x282c, 0x2928, 0x3034, 0x3130, 0x3230, 0x3334, 0x3430, 0x3534,
	0x3024, 0x3120, 0x3220, 0x3324, 0x3420, 0x3524, 0x3624, 0x3720,
	0x3828, 0x392c, 0x4010, 0x4114, 0x4214, 0x4310, 0x4414, 0x4510,
	0x4000, 0x4104, 0x4204, 0x4300, 0x4404, 0x4500, 0x4600, 0x4704,
	0x480c, 0x4908, 0x5014, 0x5110, 0x5210, 0x5314, 0x5410, 0x5514,
	0x5004, 0x5100, 0x5200, 0x5304, 0x5400, 0x5504, 0x5604, 0x5700,
	0x5808, 0x590c, 0x6034, 0x6130, 0x6230, 0x6334, 0x6430, 0x6534,
	0x6024, 0x6120, 0x6220, 0x6324, 0x6420, 0x6524, 0x6624, 0x6720,
	0x6828, 0x692c, 0x7030, 0x7134, 0x7234, 0x7330, 0x7434, 0x7530,
	0x7020, 0x7124, 0x7224, 0x7320, 0x7424, 0x7520, 0x7620, 0x7724,
	0x782c, 0x7928, 0x8090, 0x8194, 0x8294, 0x8390, 0x8494, 0x8590,
	0x8080, 0x8184, 0x8284, 0x8380, 0x8484, 0x8580, 0x8680, 0x8784,
	0x888c, 0x8988, 0x9094, 0x9190, 0x9290, 0x9394, 0x9490, 0x9594,
	0x9084, 0x9180, 0x9280, 0x9384, 0x9480, 0x9584, 0x9684, 0x9780,
	0x9888, 0x998c, 0x0055, 0x0111, 0x0211, 0x0315, 0x0411, 0x0515,
	0x0045, 0x0101, 0x0201, 0x0305, 0x0401, 0x0505, 0x0605, 0x0701,
	0x0809, 0x090d, 0x1011, 0x1115, 0x1215, 0x1311, 0x1415, 0x1511,
	0x1001, 0x1105, 0x1205, 0x1301, 0x1405, 0x1501, 0x1601, 0x1705,
	0x180d, 0x1909, 0x2031, 0x2135, 0x2235, 0x2331, 0x2435, 0x2531,
	0x2021, 0x2125, 0x2225, 0x2321, 0x2425, 0x2521, 0x2621, 0x2725,
	0x282d, 0x2929, 0x3035, 0x3131, 0x3231, 0x3335, 0x3431, 0x3535,
	0x3025, 0x3121, 0x3221, 0x3325, 0x3421, 0x3525, 0x3625, 0x3721,
	0x3829, 0x392d, 0x4011, 0x4115, 0x4215, 0x4311, 0x4415, 0x4511,
	0x4001, 0x4105, 0x4205, 0x4301, 0x4405, 0x4501, 0x4601, 0x4705,
	0x480d, 0x4909, 0x5015, 0x5111, 0x5211, 0x5315, 0x5411, 0x5515,
	0x5005, 0x5101, 0x5201, 0x5305, 0x5401, 0x5505, 0x5605, 0x5701,
	0x5809, 0x590d, 0x6035, 0x6131, 0x6231, 0x6335, 0x6431, 0x6535,
	0x6025, 0x6121, 0x6221, 0x6325, 0x6421, 0x6525, 0x6625, 0x6721,
	0x6829, 0x692d, 0x7031, 0x7135, 0x7235, 0x7331, 0x7435, 0x7531,
	0x7021, 0x7125, 0x7225, 0x7321, 0x7425, 0x7521, 0x7621, 0x7725,
	0x782d, 0x7929, 0x8091, 0x8195, 0x8295, 0x8391, 0x8495, 0x8591,
	0x8081, 0x8185, 0x8285, 0x8381, 0x8485, 0x8581, 0x8681, 0x8785,
	0x888d, 0x8989, 0x9095, 0x9191, 0x9291, 0x9395, 0x9491, 0x9595,
	0x9085, 0x9181, 0x9281, 0x9385, 0x9481, 0x9585, 0x9685, 0x9781,
	0x9889, 0x998d, 0xa0b5, 0xa1b1, 0xa2b1, 0xa3b5, 0xa4b1, 0xa5b5,
	0xa0a5, 0xa1a1, 0xa2a1, 0xa3a5, 0xa4a1, 0xa5a5, 0xa6a5, 0xa7a1,
	0xa8a9, 0xa9ad, 0xb0b1, 0xb1b5, 0xb2b5, 0xb3b1, 0xb4b5, 0xb5b1,
	0xb0a1, 0xb1a5, 0xb2a5, 0xb3a1, 0xb4a5, 0xb5a1, 0xb6a1, 0xb7a5,
	0xb8ad, 0xb9a9, 0xc095, 0xc191, 0xc291, 0xc395, 0xc491, 0xc595,
	0xc085, 0xc181, 0xc281, 0xc385, 0xc481, 0xc585, 0xc685, 0xc781,
	0xc889, 0xc98d, 0xd091, 0xd195, 0xd295, 0xd391, 0xd495, 0xd591,
	0xd081, 0xd185, 0xd285, 0xd381, 0xd485, 0xd581, 0xd681, 0xd785,
	0xd88d, 0xd989, 0xe0b1, 0xe1b5, 0xe2b5, 0xe3b1, 0xe4b5, 0xe5b1,
	0xe0a1, 0xe1a5, 0xe2a5, 0xe3a1, 0xe4a5, 0xe5a1, 0xe6a1, 0xe7a5,
	0xe8ad, 0xe9a9, 0xf0b5, 0xf1b1, 0xf2b1, 0xf3b5, 0xf4b1, 0xf5b5,
	0xf0a5, 0xf1a1, 0xf2a1, 0xf3a5, 0xf4a1, 0xf5a5, 0xf6a5, 0xf7a1,
	0xf8a9, 0xf9ad, 0x0055, 0x0111, 0x0211, 0x0315, 0x0411, 0x0515,
	0x0045, 0x0101, 0x0201, 0x0305, 0x0401, 0x0505, 0x0605, 0x0701,
	0x0809, 0x090d, 0x1011, 0x1115, 0x1215, 0x1311, 0x1415, 0x1511,
	0x1001, 0x1105, 0x1205, 0x1301, 0x1405, 0x1501, 0x1601, 0x1705,
	0x180d, 0x1909, 0x2031, 0x2135, 0x2235, 0x2331, 0x2435, 0x2531,
	0x2021, 0x2125, 0x2225, 0x2321, 0x2425, 0x2521, 0x2621, 0x2725,
	0x282d, 0x2929, 0x3035, 0x3131, 0x3231, 0x3335, 0x3431, 0x3535,
	0x3025, 0x3121, 0x3221, 0x3325, 0x3421, 0x3525, 0x3625, 0x3721,
	0x3829, 0x392d, 0x4011, 0x4115, 0x4215, 0x4311, 0x4415, 0x4511,
	0x4001, 0x4105, 0x4205, 0x4301, 0x4405, 0x4501, 0x4601, 0x4705,
	0x480d, 0x4909, 0x5015, 0x5111, 0x5211, 0x5315, 0x5411, 0x5515,
	0x5005, 0x5101, 0x5201, 0x5305, 0x5401, 0x5505, 0x5605, 0x5701,
	0x5809, 0x590d, 0x6035, 0x6131, 0x6231, 0x6335, 0x6431, 0x6535,
	0x0046, 0x0102, 0x0202, 0x0306, 0x0402, 0x0506, 0x0606, 0x0702,
	0x080a, 0x090e, 0x0402, 0x0506, 0x0606, 0x0702, 0x080a, 0x090e,
	0x1002, 0x1106, 0x1206, 0x1302, 0x1406, 0x1502, 0x1602, 0x1706,
	0x180e, 0x190a, 0x1406, 0x1502, 0x1602, 0x1706, 0x180e, 0x190a,
	0x2022, 0x2126, 0x2226, 0x2322, 0x2426, 0x2522, 0x2622, 0x2726,
	0x282e, 0x292a, 0x2426, 0x2522, 0x2622, 0x2726, 0x282e, 0x292a,
	0x3026, 0x3122, 0x3222, 0x3326, 0x3422, 0x3526, 0x3626, 0x3722,
	0x382a, 0x392e, 0x3422, 0x3526, 0x3626, 0x3722, 0x382a, 0x392e,
	0x4002, 0x4106, 0x4206, 0x4302, 0x4406, 0x4502, 0x4602, 0x4706,
	0x480e, 0x490a, 0x4406, 0x4502, 0x4602, 0x4706, 0x480e, 0x490a,
	0x5006, 0x5102, 0x5202, 0x5306, 0x5402, 0x5506, 0x5606, 0x5702,
	0x580a, 0x590e, 0x5402, 0x5506, 0x5606, 0x5702, 0x580a, 0x590e,
	0x6026, 0x6122, 0x6222, 0x6326, 0x6422, 0x6526, 0x6626, 0x6722,
	0x682a, 0x692e, 0x6422, 0x6526, 0x6626, 0x6722, 0x682a, 0x692e,
	0x7022, 0x7126, 0x7226, 0x7322, 0x7426, 0x7522, 0x7622, 0x7726,
	0x782e, 0x792a, 0x7426, 0x7522, 0x7622, 0x7726, 0x782e, 0x792a,
	0x8082, 0x8186, 0x8286, 0x8382, 0x8486, 0x8582, 0x8682, 0x8786,
	0x888e, 0x898a, 0x8486, 0x8582, 0x8682, 0x8786, 0x888e, 0x898a,
	0x9086, 0x9182, 0x9282, 0x9386, 0x9482, 0x9586, 0x9686, 0x9782,
	0x988a, 0x998e, 0x3423, 0x3527, 0x3627, 0x3723, 0x382b, 0x392f,
	0x4003, 0x4107, 0x4207, 0x4303, 0x4407, 0x4503, 0x4603, 0x4707,
	0x480f, 0x490b, 0x4407, 0x4503, 0x4603, 0x4707, 0x480f, 0x490b,
	0x5007, 0x5103, 0x5203, 0x5307, 0x5403, 0x5507, 0x5607, 0x5703,
	0x580b, 0x590f, 0x5403, 0x5507, 0x5607, 0x5703, 0x580b, 0x590f,
	0x6027, 0x6123, 0x6223, 0x6327, 0x6423, 0x6527, 0x6627, 0x6723,
	0x682b, 0x692f, 0x6423, 0x6527, 0x6627, 0x6723, 0x682b, 0x692f,
	0x7023, 0x7127, 0x7227, 0x7323, 0x7427, 0x7523, 0x7623, 0x7727,
	0x782f, 0x792b, 0x7427, 0x7523, 0x7623, 0x7727, 0x782f, 0x792b,
	0x8083, 0x8187, 0x8287, 0x8383, 0x8487, 0x8583, 0x8683, 0x8787,
	0x888f, 0x898b, 0x8487, 0x8583, 0x8683, 0x8787, 0x888f, 0x898b,
	0x9087, 0x9183, 0x9283, 0x9387, 0x9483, 0x9587, 0x9687, 0x9783,
	0x988b, 0x998f, 0x9483, 0x9587, 0x9687, 0x9783, 0x988b, 0x998f,
	0xa0a7, 0xa1a3, 0xa2a3, 0xa3a7, 0xa4a3, 0xa5a7, 0xa6a7, 0xa7a3,
	0xa8ab, 0xa9af, 0xa4a3, 0xa5a7, 0xa6a7, 0xa7a3, 0xa8ab, 0xa9af,
	0xb0a3, 0xb1a7, 0xb2a7, 0xb3a3, 0xb4a7, 0xb5a3, 0xb6a3, 0xb7a7,
	0xb8af, 0xb9ab, 0xb4a7, 0xb5a3, 0xb6a3, 0xb7a7, 0xb8af, 0xb9ab,
	0xc087, 0xc183, 0xc283, 0xc387, 0xc483, 0xc587, 0xc687, 0xc783,
	0xc88b, 0xc98f, 0xc483, 0xc587, 0xc687, 0xc783, 0xc88b, 0xc98f,
	0xd083, 0xd187, 0xd287, 0xd383, 0xd487, 0xd583, 0xd683, 0xd787,
	0xd88f, 0xd98b, 0xd487, 0xd583, 0xd683, 0xd787, 0xd88f, 0xd98b,
	0xe0a3, 0xe1a7, 0xe2a7, 0xe3a3, 0xe4a7, 0xe5a3, 0xe6a3, 0xe7a7,
	0xe8af, 0xe9ab, 0xe4a7, 0xe5a3, 0xe6a3, 0xe7a7, 0xe8af, 0xe9ab,
	0xf0a7, 0xf1a3, 0xf2a3, 0xf3a7, 0xf4a3, 0xf5a7, 0xf6a7, 0xf7a3,
	0xf8ab, 0xf9af, 0xf4a3, 0xf5a7, 0xf6a7, 0xf7a3, 0xf8ab, 0xf9af,
	0x0047, 0x0103, 0x0203, 0x0307, 0x0403, 0x0507, 0x0607, 0x0703,
	0x080b, 0x090f, 0x0403, 0x0507, 0x0607, 0x0703, 0x080b, 0x090f,
	0x1003, 0x1107, 0x1207, 0x1303, 0x1407, 0x1503, 0x1603, 0x1707,
	0x180f, 0x190b, 0x1407, 0x1503, 0x1603, 0x1707, 0x180f, 0x190b,
	0x2023, 0x2127, 0x2227, 0x2323, 0x2427, 0x2523, 0x2623, 0x2727,
	0x282f, 0x292b, 0x2427, 0x2523, 0x2623, 0x2727, 0x282f, 0x292b,
	0x3027, 0x3123, 0x3223, 0x3327, 0x3423, 0x3527, 0x3627, 0x3723,
	0x382b, 0x392f, 0x3423, 0x3527, 0x3627, 0x3723, 0x382b, 0x392f,
	0x4003, 0x4107, 0x4207, 0x4303, 0x4407, 0x4503, 0x4603, 0x4707,
	0x480f, 0x490b, 0x4407, 0x4503, 0x4603, 0x4707, 0x480f, 0x490b,
	0x5007, 0x5103, 0x5203, 0x5307, 0x5403, 0x5507, 0x5607, 0x5703,
	0x580b, 0x590f, 0x5403, 0x5507, 0x5607, 0x5703, 0x580b, 0x590f,
	0x6027, 0x6123, 0x6223, 0x6327, 0x6423, 0x6527, 0x6627, 0x6723,
	0x682b, 0x692f, 0x6423, 0x6527, 0x6627, 0x6723, 0x682b, 0x692f,
	0x7023, 0x7127, 0x7227, 0x7323, 0x7427, 0x7523, 0x7623, 0x7727,
	0x782f, 0x792b, 0x7427, 0x7523, 0x7623, 0x7727, 0x782f, 0x792b,
	0x8083, 0x8187, 0x8287, 0x8383, 0x8487, 0x8583, 0x8683, 0x8787,
	0x888f, 0x898b, 0x8487, 0x8583, 0x8683, 0x8787, 0x888f, 0x898b,
	0x9087, 0x9183, 0x9283, 0x9387, 0x9483, 0x9587, 0x9687, 0x9783,
	0x988b, 0x998f, 0x9483, 0x9587, 0x9687, 0x9783, 0x988b, 0x998f,
	0x0604, 0x0700, 0x0808, 0x090c, 0x0a0c, 0x0b08, 0x0c0c, 0x0d08,
	0x0e08, 0x0f0c, 0x1010, 0x1114, 0x1214, 0x1310, 0x1414, 0x1510,
	0x1600, 0x1704, 0x180c, 0x1908, 0x1a08, 0x1b0c, 0x1c08, 0x1d0c,
	0x1e0c, 0x1f08, 0x2030, 0x2134, 0x2234, 0x2330, 0x2434, 0x2530,
	0x2620, 0x2724, 0x282c, 0x2928, 0x2a28, 0x2b2c, 0x2c28, 0x2d2c,
	0x2e2c, 0x2f28, 0x3034, 0x3130, 0x3230, 0x3334, 0x3430, 0x3534,
	0x3624, 0x3720, 0x3828, 0x392c, 0x3a2c, 0x3b28, 0x3c2c, 0x3d28,
	0x3e28, 0x3f2c, 0x4010, 0x4114, 0x4214, 0x4310, 0x4414, 0x4510,
	0x4600, 0x4704, 0x480c, 0x4908, 0x4a08, 0x4b0c, 0x4c08, 0x4d0c,
	0x4e0c, 0x4f08, 0x5014, 0x5110, 0x5210, 0x5314, 0x5410, 0x5514,
	0x5604, 0x5700, 0x5808, 0x590c, 0x5a0c, 0x5b08, 0x5c0c, 0x5d08,
	0x5e08, 0x5f0c, 0x6034, 0x6130, 0x6230, 0x6334, 0x6430, 0x6534,
	0x6624, 0x6720, 0x6828, 0x692c, 0x6a2c, 0x6b28, 0x6c2c, 0x6d28,
	0x6e28, 0x6f2c, 0x7030, 0x7134, 0x7234, 0x7330, 0x7434, 0x7530,
	0x7620, 0x7724, 0x782c, 0x7928, 0x7a28, 0x7b2c, 0x7c28, 0x7d2c,
	0x7e2c, 0x7f28, 0x8090, 0x8194, 0x8294, 0x8390, 0x8494, 0x8590,
	0x8680, 0x8784, 0x888c, 0x8988, 0x8a88, 0x8b8c, 0x8c88, 0x8d8c,
	0x8e8c, 0x8f88, 0x9094, 0x9190, 0x9290, 0x9394, 0x9490, 0x9594,
	0x9684, 0x9780, 0x9888, 0x998c, 0x9a8c, 0x9b88, 0x9c8c, 0x9d88,
	0x9e88, 0x9f8c, 0x0055, 0x0111, 0x0211, 0x0315, 0x0411, 0x0515,
	0x0605, 0x0701, 0x0809, 0x090d, 0x0a0d, 0x0b09, 0x0c0d, 0x0d09,
	0x0e09, 0x0f0d, 0x1011, 0x1115, 0x1215, 0x1311, 0x1415, 0x1511,
	0x1601, 0x1705, 0x180d, 0x1909, 0x1a09, 0x1b0d, 0x1c09, 0x1d0d,
	0x1e0d, 0x1f09, 0x2031, 0x2135, 0x2235, 0x2331, 0x2435, 0x2531,
	0x2621, 0x2725, 0x282d, 0x2929, 0x2a29, 0x2b2d, 0x2c29, 0x2d2d,
	0x2e2d, 0x2f29, 0x3035, 0x3131, 0x3231, 0x3335, 0x3431, 0x3535,
	0x3625, 0x3721, 0x3829, 0x392d, 0x3a2d, 0x3b29, 0x3c2d, 0x3d29,
	0x3e29, 0x3f2d, 0x4011, 0x4115, 0x4215, 0x4311, 0x4415, 0x4511,
	0x4601, 0x4705, 0x480d, 0x4909, 0x4a09, 0x4b0d, 0x4c09, 0x4d0d,
	0x4e0d, 0x4f09, 0x5015, 0x5111, 0x5211, 0x5315, 0x5411, 0x5515,
	0x5605, 0x5701, 0x5809, 0x590d, 0x5a0d, 0x5b09, 0x5c0d, 0x5d09,
	0x5e09, 0x5f0d, 0x6035, 0x6131, 0x6231, 0x6335, 0x6431, 0x6535,
	0x6625, 0x6721, 0x6829, 0x692d, 0x6a2d, 0x6b29, 0x6c2d, 0x6d29,
	0x6e29, 0x6f2d, 0x7031, 0x7135, 0x7235, 0x7331, 0x7435, 0x7531,
	0x7621, 0x7725, 0x782d, 0x7929, 0x7a29, 0x7b2d, 0x7c29, 0x7d2d,
	0x7e2d, 0x7f29, 0x8091, 0x8195, 0x8295, 0x8391, 0x8495, 0x8591,
	0x8681, 0x8785, 0x888d, 0x8989, 0x8a89, 0x8b8d, 0x8c89, 0x8d8d,
	0x8e8d, 0x8f89, 0x9095, 0x9191, 0x9291, 0x9395, 0x9491, 0x9595,
	0x9685, 0x9781, 0x9889, 0x998d, 0x9a8d, 0x9b89, 0x9c8d, 0x9d89,
	0x9e89, 0x9f8d, 0xa0b5, 0xa1b1, 0xa2b1, 0xa3b5, 0xa4b1, 0xa5b5,
	0xa6a5, 0xa7a1, 0xa8a9, 0xa9ad, 0xaaad, 0xaba9, 0xacad, 0xada9,
	0xaea9, 0xafad, 0xb0b1, 0xb1b5, 0xb2b5, 0xb3b1, 0xb4b5, 0xb5b1,
	0xb6a1, 0xb7a5, 0xb8ad, 0xb9a9, 0xbaa9, 0xbbad, 0xbca9, 0xbdad,
	0xbead, 0xbfa9, 0xc095, 0xc191, 0xc291, 0xc395, 0xc491, 0xc595,
	0xc685, 0xc781, 0xc889, 0xc98d, 0xca8d, 0xcb89, 0xcc8d, 0xcd89,
	0xce89, 0xcf8d, 0xd091, 0xd195, 0xd295, 0xd391, 0xd495, 0xd591,
	0xd681, 0xd785, 0xd88d, 0xd989, 0xda89, 0xdb8d, 0xdc89, 0xdd8d,
	0xde8d, 0xdf89, 0xe0b1, 0xe1b5, 0xe2b5, 0xe3b1, 0xe4b5, 0xe5b1,
	0xe6a1, 0xe7a5, 0xe8ad, 0xe9a9, 0xeaa9, 0xebad, 0xeca9, 0xedad,
	0xeead, 0xefa9, 0xf0b5, 0xf1b1, 0xf2b1, 0xf3b5, 0xf4b1, 0xf5b5,
	0xf6a5, 0xf7a1, 0xf8a9, 0xf9ad, 0xfaad, 0xfba9, 0xfcad, 0xfda9,
	0xfea9, 0xffad, 0x0055, 0x0111, 0x0211, 0x0315, 0x0411, 0x0515,
	0x0605, 0x0701, 0x0809, 0x090d, 0x0a0d, 0x0b09, 0x0c0d, 0x0d09,
	0x0e09, 0x0f0d, 0x1011, 0x1115, 0x1215, 0x1311, 0x1415, 0x1511,
	0x1601, 0x1705, 0x180d, 0x1909, 0x1a09, 0x1b0d, 0x1c09, 0x1d0d,
	0x1e0d, 0x1f09, 0x2031, 0x2135, 0x2235, 0x2331, 0x2435, 0x2531,
	0x2621, 0x2725, 0x282d, 0x2929, 0x2a29, 0x2b2d, 0x2c29, 0x2d2d,
	0x2e2d, 0x2f29, 0x3035, 0x3131, 0x3231, 0x3335, 0x3431, 0x3535,
	0x3625, 0x3721, 0x3829, 0x392d, 0x3a2d, 0x3b29, 0x3c2d, 0x3d29,
	0x3e29, 0x3f2d, 0x4011, 0x4115, 0x4215, 0x4311, 0x4415, 0x4511,
	0x4601, 0x4705, 0x480d, 0x4909, 0x4a09, 0x4b0d, 0x4c09, 0x4d0d,
	0x4e0d, 0x4f09, 0x5015, 0x5111, 0x5211, 0x5315, 0x5411, 0x5515,
	0x5605, 0x5701, 0x5809, 0x590d, 0x5a0d, 0x5b09, 0x5c0d, 0x5d09,
	0x5e09, 0x5f0d, 0x6035, 0x6131, 0x6231, 0x6335, 0x6431, 0x6535,
	0xfabe, 0xfbba, 0xfcbe, 0xfdba, 0xfeba, 0xffbe, 0x0046, 0x0102,
	0x0202, 0x0306, 0x0402, 0x0506, 0x0606, 0x0702, 0x080a, 0x090e,
	0x0a1e, 0x0b1a, 0x0c1e, 0x0d1a, 0x0e1a, 0x0f1e, 0x1002, 0x1106,
	0x1206, 0x1302, 0x1406, 0x1502, 0x1602, 0x1706, 0x180e, 0x190a,
	0x1a1a, 0x1b1e, 0x1c1a, 0x1d1e, 0x1e1e, 0x1f1a, 0x2022, 0x2126,
	0x2226, 0x2322, 0x2426, 0x2522, 0x2622, 0x2726, 0x282e, 0x292a,
	0x2a3a, 0x2b3e, 0x2c3a, 0x2d3e, 0x2e3e, 0x2f3a, 0x3026, 0x3122,
	0x3222, 0x3326, 0x3422, 0x3526, 0x3626, 0x3722, 0x382a, 0x392e,
	0x3a3e, 0x3b3a, 0x3c3e, 0x3d3a, 0x3e3a, 0x3f3e, 0x4002, 0x4106,
	0x4206, 0x4302, 0x4406, 0x4502, 0x4602, 0x4706, 0x480e, 0x490a,
	0x4a1a, 0x4b1e, 0x4c1a, 0x4d1e, 0x4e1e, 0x4f1a, 0x5006, 0x5102,
	0x5202, 0x5306, 0x5402, 0x5506, 0x5606, 0x5702, 0x580a, 0x590e,
	0x5a1e, 0x5b1a, 0x5c1e, 0x5d1a, 0x5e1a, 0x5f1e, 0x6026, 0x6122,
	0x6222, 0x6326, 0x6422, 0x6526, 0x6626, 0x6722, 0x682a, 0x692e,
	0x6a3e, 0x6b3a, 0x6c3e, 0x6d3a, 0x6e3a, 0x6f3e, 0x7022, 0x7126,
	0x7226, 0x7322, 0x7426, 0x7522, 0x7622, 0x7726, 0x782e, 0x792a,
	0x7a3a, 0x7b3e, 0x7c3a, 0x7d3e, 0x7e3e, 0x7f3a, 0x8082, 0x8186,
	0x8286, 0x8382, 0x8486, 0x8582, 0x8682, 0x8786, 0x888e, 0x898a,
	0x8a9a, 0x8b9e, 0x8c9a, 0x8d9e, 0x8e9e, 0x8f9a, 0x9086, 0x9182,
	0x9282, 0x9386, 0x3423, 0x3527, 0x3627, 0x3723, 0x382b, 0x392f,
	0x3a3f, 0x3b3b, 0x3c3f, 0x3d3b, 0x3e3b, 0x3f3f, 0x4003, 0x4107,
	0x4207, 0x4303, 0x4407, 0x4503, 0x4603, 0x4707, 0x480f, 0x490b,
	0x4a1b, 0x4b1f, 0x4c1b, 0x4d1f, 0x4e1f, 0x4f1b, 0x5007, 0x5103,
	0x5203, 0x5307, 0x5403, 0x5507, 0x5607, 0x5703, 0x580b, 0x590f,
	0x5a1f, 0x5b1b, 0x5c1f, 0x5d1b, 0x5e1b, 0x5f1f, 0x6027, 0x6123,
	0x6223, 0x6327, 0x6423, 0x6527, 0x6627, 0x6723, 0x682b, 0x692f,
	0x6a3f, 0x6b3b, 0x6c3f, 0x6d3b, 0x6e3b, 0x6f3f, 0x7023, 0x7127,
	0x7227, 0x7323, 0x7427, 0x7523, 0x7623, 0x7727, 0x782f, 0x792b,
	0x7a3b, 0x7b3f, 0x7c3b, 0x7d3f, 0x7e3f, 0x7f3b, 0x8083, 0x8187,
	0x8287, 0x8383, 0x8487, 0x8583, 0x8683, 0x8787, 0x888f, 0x898b,
	0x8a9b, 0x8b9f, 0x8c9b, 0x8d9f, 0x8e9f, 0x8f9b, 0x9087, 0x9183,
	0x9283, 0x9387, 0x9483, 0x9587, 0x9687, 0x9783, 0x988b, 0x998f,
	0x9a9f, 0x9b9b, 0x9c9f, 0x9d9b, 0x9e9b, 0x9f9f, 0xa0a7, 0xa1a3,
	0xa2a3, 0xa3a7, 0xa4a3, 0xa5a7, 0xa6a7, 0xa7a3, 0xa8ab, 0xa9af,
	0xaabf, 0xabbb, 0xacbf, 0xadbb, 0xaebb, 0xafbf, 0xb0a3, 0xb1a7,
	0xb2a7, 0xb3a3, 0xb4a7, 0xb5a3, 0xb6a3, 0xb7a7, 0xb8af, 0xb9ab,
	0xbabb, 0xbbbf, 0xbcbb, 0xbdbf, 0xbebf, 0xbfbb, 0xc087, 0xc183,
	0xc283, 0xc387, 0xc483, 0xc587, 0xc687, 0xc783, 0xc88b, 0xc98f,
	0xca9f, 0xcb9b, 0xcc9f, 0xcd9b, 0xce9b, 0xcf9f, 0xd083, 0xd187,
	0xd287, 0xd383, 0xd487, 0xd583, 0xd683, 0xd787, 0xd88f, 0xd98b,
	0xda9b, 0xdb9f, 0xdc9b, 0xdd9f, 0xde9f, 0xdf9b, 0xe0a3, 0xe1a7,
	0xe2a7, 0xe3a3, 0xe4a7, 0xe5a3, 0xe6a3, 0xe7a7, 0xe8af, 0xe9ab,
	0xeabb, 0xebbf, 0xecbb, 0xedbf, 0xeebf, 0xefbb, 0xf0a7, 0xf1a3,
	0xf2a3, 0xf3a7, 0xf4a3, 0xf5a7, 0xf6a7, 0xf7a3, 0xf8ab, 0xf9af,
	0xfabf, 0xfbbb, 0xfcbf, 0xfdbb, 0xfebb, 0xffbf, 0x0047, 0x0103,
	0x0203, 0x0307, 0x0403, 0x0507, 0x0607, 0x0703, 0x080b, 0x090f,
	0x0a1f, 0x0b1b, 0x0c1f, 0x0d1b, 0x0e1b, 0x0f1f, 0x1003, 0x1107,
	0x1207, 0x1303, 0x1407, 0x1503, 0x1603, 0x1707, 0x180f, 0x190b,
	0x1a1b, 0x1b1f, 0x1c1b, 0x1d1f, 0x1e1f, 0x1f1b, 0x2023, 0x2127,
	0x2227, 0x2323, 0x2427, 0x2523, 0x2623, 0x2727, 0x282f, 0x292b,
	0x2a3b, 0x2b3f, 0x2c3b, 0x2d3f, 0x2e3f, 0x2f3b, 0x3027, 0x3123,
	0x3223, 0x3327, 0x3423, 0x3527, 0x3627, 0x3723, 0x382b, 0x392f,
	0x3a3f, 0x3b3b, 0x3c3f, 0x3d3b, 0x3e3b, 0x3f3f, 0x4003, 0x4107,
	0x4207, 0x4303, 0x4407, 0x4503, 0x4603, 0x4707, 0x480f, 0x490b,
	0x4a1b, 0x4b1f, 0x4c1b, 0x4d1f, 0x4e1f, 0x4f1b, 0x5007, 0x5103,
	0x5203, 0x5307, 0x5403, 0x5507, 0x5607, 0x5703, 0x580b, 0x590f,
	0x5a1f, 0x5b1b, 0x5c1f, 0x5d1b, 0x5e1b, 0x5f1f, 0x6027, 0x6123,
	0x6223, 0x6327, 0x6423, 0x6527, 0x6627, 0x6723, 0x682b, 0x692f,
	0x6a3f, 0x6b3b, 0x6c3f, 0x6d3b, 0x6e3b, 0x6f3f, 0x7023, 0x7127,
	0x7227, 0x7323, 0x7427, 0x7523, 0x7623, 0x7727, 0x782f, 0x792b,
	0x7a3b, 0x7b3f, 0x7c3b, 0x7d3f, 0x7e3f, 0x7f3b, 0x8083, 0x8187,
	0x8287, 0x8383, 0x8487, 0x8583, 0x8683, 0x8787, 0x888f, 0x898b,
	0x8a9b, 0x8b9f, 0x8c9b, 0x8d9f, 0x8e9f, 0x8f9b, 0x9087, 0x9183,
	0x9283, 0x9387, 0x9483, 0x9587, 0x9687, 0x9783, 0x988b, 0x998f
};
