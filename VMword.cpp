/*******************************************************************************
 * ChipKitForthgn
 * Interactive Forth environment for PIC32 based ChipKit boards.
 * Based on DIOSFORTH. http://www.forth.cz/Download/DIOSForth/DIOSForth.html
 * Developed under MPIDE.
 * Public repository: https://github.com/jvvood/ChipKitForth
 * Published under GPLv3.
 * Created by Janos Waldhauser (2013).
 * Email: janos.waldhauser@gmail.com
 ******************************************************************************/
 


/*******************************************************************************
 * This file contains base dictionary functions and tables
 ******************************************************************************/

#include <plib.h>
#include<p32xxxx.h>
#include <errno.h>
#include <stdarg.h>

#include "WProgram.h"
#include "GenericTypeDefs.h"
#include "Config.h"			// devhead  devcall
#include "VMcore.h"
#include "VMword.h"
#include "Uw.h"
#include "isr.h"


#ifdef WITH_EEPROM
#include <EEPROM.h>
#endif


// Forward references
void linkg(void);
void dotstring(void);
void crf(void);
void tick(void);
void udotr(void);
void ver(void);
void udot(void);
void clearExceptionInfo(void);
void over(void);
void equals(void);
void drop(void);
void iff(void);
void thenf(void);
void elsef(void);
void abortf(void);
void compile(void);
void dotof(void);
void typef(void);

// !!! WJ !!! need casts for MPIDE
const void *xt_over=(const void*)over;
const void *xt_equal=(const void*)equals;
const void *xt_drop=(const void*)drop;
const void *xt_if=(const void*)iff;
const void *xt_then=(const void*)thenf;
const void *xt_else=(const void*)elsef;
const void *xt_abort=(const void*)abortf;
const void *xt_compile=(const void*)compile;
const void *xt_doto=(const void*)dotof;
const void *xt_type=(const void*)typef;


SYSVARS sysvars;
char  vTib[tibsize+4], vPad[padsize+4];
char  vDict[dictsize];
//char  *pDict=vDict, *pMem=vMem;
char *vHere, *vHead;
char *vHereBak, *vHeadBak;
unsigned char  vIN=0;
unsigned char vSharpTib=0;
unsigned char vBase=DEFAULT_BASE;
unsigned char vState=0;
unsigned char vErrors=0;
WORD  PrimLast=0;
unsigned char  AddrRAM;
const char StrVer[]=VerVM;
/*
 *********** Console I/O **********
 */


void f_putdec(int x) {
  Serial.print(x, DEC);
}

int f_putc(int c) {
    int i;
   Serial.write(c);
   if (c == '\n') {
     Serial.write('\r');
   }
}

void f_puthex(UINT x, int l) {
  static char table[] = {"0123456789ABCDEF"};
  static char out[16];
  int i;
  char c;
  for(i=0;i<l;++i) {
    c = (x & 0x0f);
    x = x >> 4;
    out[i] = table[c];
  }
  for(i=l-1;i>=0;--i) {
    f_putc(out[i]);
  }
}




int f_getc(void) {
  char received;
  if ( (autoload_ptr) && (*autoload_ptr)) {
    received = *autoload_ptr;
    ++autoload_ptr;
    return received;
  } else {
      while (1) {
#ifdef WITH_ISR    
         isrw();
#endif        
          received = Serial.read();
          if (received != -1) {
            return received;
          }
      }
  }
}


void f_puts(char *p) {
  while (*p) {
    f_putc(*p);
    ++p;
  }
}



/*
 *********** FLASH erase/write **********
 */


// Törli a paraméterként kapott címet tartalmazó lapot.
// hiba esetén 0-val, egyébként -1-el tér vissza.
char NVMerase(UINT *pAddr)
{
//Serial.print("NVMerase: ");
//Serial.println((uint32_t)pAddr, HEX);
        pAddr = (UINT*)((UINT)pAddr & 0xdfffffff);
	UINT i;
        UINT Ad;
        Ad=(UINT)pAddr & ~(PAGE_SIZE-1);

	if (Ad>FREE_FLASH_END) {
          // Serial.print("NVMerase return1: 0.");
          return 0;
        }
//Serial.print("NVMErase1:");
//Serial.println((INT)Ad, HEX);
	NVMErasePage((void *)Ad); 

	for(i=0; i<PAGE_SIZE; i+=4) {
          if (*(int *)(Ad+i)!=-1) {
            // Serial.print("NVMerase return2: 0.");
            return 0;
          }
        }
        // Serial.print("NVMerase return3: -1.");
	return -1;
}


// Egy word-öt ír a megadott címre.
// Ha a cím egy lap első címe, akkor írás előtt törli azt.
// Írás után ellenőriz.
// Hiba esetén 0-val, egyébként -1-el tér vissza.
char NVMwrite(UINT *pAddr, UINT Data)
{
  pAddr = (UINT*)((UINT)pAddr & 0xdfffffff);
  // Serial.print("NVMwrite: ");
  // Serial.print((uint32_t)pAddr, HEX);
  // Serial.print(" : ");
  // Serial.println((uint32_t)Data, HEX);
  
	UINT Addr=(UINT)pAddr;

	if (Addr>FREE_FLASH_END) {
          // Serial.print("NVMwrite return1 0.");
          return 0;
        }
//	if ( ! (Addr & ~(PAGE_SIZE-1)) ) { // Erase new page
//          if (!NVMerase(pAddr)) {
//Serial.print("NVMwrite return2 0.");
//            return 0;
//          }
//        }  
// Serial.print("NVMWriteWord:");        
// Serial.print((int)pAddr, HEX);
// Serial.print(" : ");        
// Serial.print((int)Data, HEX);
// Serial.println();
	NVMWriteWord(pAddr, Data); 
        if (*pAddr!=Data) {
          // Serial.print("NVMwrite return3 0.");
//          return 0;
        }  // Write and compare
        // Serial.print("NVMwrite return4 -1.");
	return -1;
}





// ********** C UTIL **********

void FindLastC(void)  // Find last primitive word
{
	WORD i=0;
	while(primwords[i].wlink!=0xFF) {
          i++;
        }
	PrimLast=--i;
}


void CompileCxt(WORD i)  // compile xt primitive for C
{
	PUSH((ucell)&primwords[i].wcall); 
        comma(); // XT ,
}


void CompileCpfa(WORD i)  // compile pfa primitive for C
{
	PUSH((ucell)primwords[i].wcall); 
        comma();  // PFA ,
}


void CompileCcon(void *i)  // compile const for C
{
	PUSH((ucell)i); 
        comma(); // x ,
}


// ********** STACK **********

// drop ( x --  )
void drop(void) {
  pDS--;
}


// 2drop ( x1 x2 --  )
void twodrop(void) {
  pDS-=2;
}


// dup (x1 -- x1 x1 )
void dup(void) {
  cell tmp=TOS; 
  PUSH(tmp);
}


// 2dup ( x1 x2 -- x1 x2 x1 x2 )
void twodup(void) {
  cell tmp=TOSi(1); 
  PUSH(tmp); 
  tmp=TOSi(1); 
  PUSH(tmp);
}


// ?dup ( x -- 0 | x x )
void isdup(void) {
  cell tmp=TOS;	
  if (tmp) {
    PUSH(tmp);
  }
}


// nip ( x1 x2 -- x2 )
void nip(void) {
  TOSi(1)=TOS; 
  pDS--;
}


// over ( x y -- x y x )
void over(void) {
  cell tmp=TOSi(1); 
  PUSH(tmp);
}


// 2over ( x1 x2 x3 x4 -- x1 x2 x3 x4 x1 x2 )
void twoover(void) {
  cell tmp=TOSi(3); 
  PUSH(tmp); 
  tmp=TOSi(3); 
  PUSH(tmp);
}


// pick ( xu ... x1 x0 u -- xu ... x1 x0 xu )  copy xu
void pick(void) {
  ucell u=TOS; 
  if (DScnt>u+1) {
    TOS=TOSi(u+1);
  } else {
    vErrors|=4; 
    abortf();
  }
}


// stick ( xu ... x1 x0 xunew u -- xunew ... x1 x0 )  overwrite xu
void stick(void) {
  cell u=POP; 
  cell n_ew=POP; 
  if (DScnt>u) {
    TOSi(u)=n_ew;
  } else {
    vErrors|=4; 
    abortf();
  }
}


// roll ( xu ... x1 x0 u -- xu-1... x1 x0 xu )  move xu to TOS
void roll(void)
{
	ucell u=POP, *p=(ucell *)pDSzero;
	cell cnt=DScnt;

	if (!cnt) {
          return;
        }
	if (cnt>u) {
		int i;
		cell xu=TOSi(u);
		for (i=cnt-u; i<cnt; i++) {
                  *(pDSzero+i)=*(pDSzero+i+1);
                }
		TOS=xu;
	} else {
          vErrors|=4; 
          abortf();
        }
}


// -roll ( xu ... x1 x0 u -- x0 xu... x1 )  insert TOS to xu
void minusroll(void)
{
	ucell u=POP;
	cell cnt=DScnt;

	if (!cnt) {
         return;
        }
	if (cnt>u) {
		int i;
		cell x0=TOS;
		for (i=cnt; i>cnt-u; i--) {
                  *(pDSzero+i)=*(pDSzero+i-1);
                }
		TOSi(u)=x0;
	} else {
          vErrors|=4; 
          abortf();
        }
}


// rot ( x1 x2 x3 -- x2 x3 x1 )
void rot(void) {
  cell tmp=TOS; 
  TOS=TOSi(2); 
  TOSi(2)=TOSi(1); 
  TOSi(1)=tmp;
}


// -rot ( x1 x2 x3 -- x3 x1 x2 )
void minusrot(void) {
  cell tmp=TOS; 
  TOS=TOSi(1); 
  TOSi(1)=TOSi(2); 
  TOSi(2)=tmp;
}


// swap ( x1 x2 -- x2 x1 )
void swap(void) {
  cell tmp=TOS; 
  TOS=TOSi(1); 
  TOSi(1)=tmp;
}


// 2swap ( x1 x2 x3 x4 -- x3 x4 x1 x2 )
void twoswap(void)
{
	cell tmp=TOS;
        cell tmp2=TOSi(1);
	TOS=TOSi(2); 
        TOSi(1)=TOSi(3);
	TOSi(2)=tmp; 
        TOSi(3)=tmp2;
}


// >r ( x -- ) (R: -- x ) 
void tor(void) {
  PUSHR(POP);
}


// r> ( -- x ) (R: x -- )
void rfrom(void) {
  PUSH(POPR);
}


// r@ ( -- x ) (R: x -- x )
void rfetch(void) {
  PUSH(TOSR);
}


// depth (  -- x )
void depth(void) {
  cell tmp=DScnt; 
  PUSH(tmp);
}


// depth! ( ... u --  x1 x2 .. xu )
void depthwrite(void) {
  ucell tmp=POP; 
  pDS=pDSzero+tmp;
}

// sp0
void spzero(void) {
  pDS = pDSzero;
}

// rdepth (  -- u )
void rdepth(void) {
  cell tmp=RScnt; 
  PUSH(tmp);
}


// rdepth! ( u --  ) ( R: ... -- x1 x2 .. xu )
void rdepthwrite(void) {
  ucell tmp=POP; 
  pRS=pRSzero+tmp;
}

// rp0
void rpzero(void) {
  pRS = pRSzero;
}


// sp@ (  -- addr )
void spfetch(void) {
  ucell tmp=(ucell)&TOS; 
  PUSH(tmp);
}


// rp@ (  -- u )
void rpfetch(void) {
  ucell tmp=(ucell)&TOSR; 
  PUSH(tmp);
}


// ********** OTHER **********


// nop ( -- )
void nop(void) { 
}


// emit ( char --  )
void emit(void)
{
  if ( lcd_active ) {
    lcd_emit();
  } else {
    f_putc(POP);
  }
}





// key (  -- char )
void key(void)
{
//  int c;
//  while ( (c=Serial.read()) == -1) {
//    delay(10);
//  }
  PUSH(f_getc());
} 


// ?key (  -- pressed? )
void iskey(void) {
  PUSH(Serial.available());
}


// i ( -- i )
void loop_i(void) {
  PUSH(TOSR);
}


// j ( -- j )
void loop_j(void) {
  PUSH(TOSRi(3));
}


// k ( -- k )
void loop_k(void) {
  PUSH(TOSRi(6));
}


// ********** MEMORY **********


// @ ( addr -- x )
void fetch(void) {
  TOS=pDATA TOS;
}


// c@ ( addr -- byte )
void cfetch(void) {
  TOS=*(unsigned char *)TOS;
}


// w@ ( addr -- w )
void wfetch(void) {
  TOS=*(WORD *)TOS;
}


// ! ( x addr -- )
void store(void) {
  ucell addr=POP; 
  pDATA addr=POP;
}


// c! ( byte addr -- )
void cstore(void) {
  ucell addr=POP; 
  *(unsigned char *) addr=POP;
}


// w! ( w addr -- )
void wstore(void) {
  ucell addr=POP; 
  *(WORD *) addr=POP;
}


// +! ( n addr -- )
void plusstore(void) {
  ucell addr=POP; 
  pDATA addr+=POP;
}


// fill ( addr len c -- )
void fillf(void) { // fill c
	ucell value=POP;
        ucell count=POP;
        ucell src=POP;
	memset((char *)src, value, count);
}


// move ( src-addr dest-addr len -- )
void movef(void) { // move c
	ucell count=POP;
        ucell dest=POP;
        ucell src=POP;
	memmove((char *)dest, (char *)src, count);
}


// here ( -- u )
void here(void) {
  PUSH((ucell)vHere);
}

// head ( -- u )
void head(void) {
  PUSH((ucell)vHead);
}


// here! ( u -- )
void herewrite(void) {
  vHere=(char *)POP;
}  // set vHere






// ********** ARITHMETIC **********


// + ( n1 n2 -- n )
void plus(void) {
  cell n2=POP; 
  TOS=TOS+n2;
}


// - ( n1 n2 -- n )
void minus(void) {
  cell n2=POP; 
  TOS=TOS-n2;
}


// d+ ( d1 d2 -- d )
void dplus(void) {
	dcell d1, d2;
	DPOP(d2); 
        DPOP(d1);
	DPUSH(d1+d2);
}


// d- ( d1 d2 -- d )
void dminus(void) {
	dcell d1, d2;
	DPOP(d2); 
        DPOP(d1);
	DPUSH(d1-d2);
}


// * ( n1 n2 -- n )
void mult(void) {
  cell n2=POP; 
  TOS=TOS*n2;
}


// u* ( u1 u2 -- u ) same as mult
//void umult(void) {ucell u2=POP; TOS=(ucell)TOS*u2;}


// m* ( n1 n2 -- d )
void mmult(void) {
  cell n2=POP;
  cell n1=POP;	
  DPUSH((dcell)n1*n2);
}


// um* ( u1 u2 -- ud )
void ummult(void) {
  ucell u2=POP;
  ucell u1=POP; 
  DPUSH((udcell)u1*u2);
}


/*
// sqrt ( u1 -- u )
#pragma message "\t\t\tIgnored: sqrtu"
//void sqrtu(void) {ucell u=TOS; TOS=sqrtf(u);}
*/


// um/mod ( ud u1 -- rem u )
void umdivmod(void) {
	udcell ud;
        udcell u1=POP;
	if (u1==0) {
          vErrors|=8; 
          abortf();
        } else {
          DPOP(ud); 
          PUSH(ud%u1); 
          PUSH(ud/u1);
        }
} 


// m/mod ( d n1 -- rem n )
void mdivmod(void) {
	dcell d;
        dcell n1=POP;
	if (n1==0) {
          vErrors|=8; 
          abortf();
        } else {
          DPOP(d); 
          PUSH(d%n1); 
          PUSH(d/n1);
        }
} 


// u/mod ( u1 u2 -- rem u )
void udivmod(void) {
	ucell u2=TOS;
        ucell u1=TOSi(1);
	if (u2==0) {
          vErrors|=8; 
          abortf();
        } else {
          TOSi(1)=u1%u2; 
          TOS=u1/u2;
        }
} 


// /mod ( n1 n2 -- rem n )
void divmod(void) {
	cell n2=TOS;
        cell n1=TOSi(1);
	if (n2==0) {
          vErrors|=8; 
          abortf();
        } else {
          TOSi(1)=n1%n2; 
          TOS=n1/n2;
        }
} 


//  / ( n1 n2 -- n )
void divf(void) {
	cell n2=POP;
        cell n1=TOS;
	if (n2==0) {
          vErrors|=8; 
          abortf();
        } else {
          TOS=n1/n2;
        }
} 


// mod ( n1 n2 -- rem )
void modn(void) {
	cell n2=POP;
        cell n1=TOS;
	if (n2==0) {
          vErrors|=8; 
          abortf();
        } else {
          TOS=n1%n2;
        }
}


// u*/ ( u1 u2 u3 -- u )
void umuldiv(void)
{
	udcell u3=POP;
        udcell u2=POP;
        udcell u1=POP;
	if (u3==0) {
          vErrors|=8; 
          abortf();
        } else {
          PUSH((u1*u2)/u3);
        }
}


// */ ( n1 n2 n3 -- n )
void muldiv(void) {
	dcell n3=POP;
        dcell n2=POP;
        dcell n1=POP;
	if (n3==0) {
          vErrors|=8; 
          abortf();
        } else {
          PUSH((n1*n2)/n3);
        }
}


// >>a ( n1 u -- n )
void arshift(void) {
  ucell u=POP; 
  TOS=(cell)TOS>>u;
}


// >> ( x1 u -- x )
void rshift(void) {
  ucell u=POP; 
  TOS=(ucell)TOS>>u;
}


// << ( x1 u -- x )
void lshift(void) {
  ucell u=POP; 
  TOS=(ucell)TOS<<u;
}


// 2* ( n1 -- n )
void twomul(void) {
  TOS=TOS<<1;
}


// 2/ ( n1 -- n )
void twodiv(void) {
  TOS=TOS>>1;
}


// min ( n1 n2 -- n1|n2 )
void minf(void) {
  cell n2=POP; 
  TOS=(n2<TOS) ? n2:TOS;
}


// max ( n1 n2 -- n1|n2 )
void maxf(void) {
  cell n2=POP; 
  TOS=(n2>TOS) ? n2:TOS;
}


// abs ( n -- u )
void absf(void) {
  TOS=(TOS<0) ? -TOS:TOS;
}


// 1+ ( n1 -- n )
void incf(void) {
  TOS++;
} 


// 1- ( n1 -- n )
void decf(void) {
  TOS--;
} 


// negate ( n1 -- n )
void negate(void) {
  TOS=-TOS;
} 


// invert ( n1 -- n )
void invert(void) {
  TOS=TOS^-1;
} 


// and ( x1 x2 -- x )
void andf(void) {
  cell x2=POP; 
  TOS=TOS&x2;
}


// or ( x1 x2 -- x )
void orf(void) {
  cell x2=POP; 
  TOS=TOS|x2;
}


// xor ( x1 x2 -- x )
void xorf(void) {
  cell x2=POP; 
  TOS=TOS^x2;
}


// ********** LOGIC **********


// andl ( x1 x2 -- fl )
void andl(void) {
  cell x2=POP; 
  TOS=(x2&&TOS) ? -1:0;;
}


// orl ( x1 x2 -- fl )
void orl(void) {
  cell x2=POP; 
  TOS=(x2||TOS) ? -1:0;
}


// not ( x1 -- fl )
void notl(void) {
  TOS=(TOS) ? 0:-1;
}


// = ( n1 n2 -- fl )
void equals(void) {
  cell n2=POP;	
  TOS=(TOS==n2) ? -1:0;
}


// <> ( n1 n2 -- fl )
void notequals(void) {
  cell n2=POP; 
  TOS=(TOS!=n2) ? -1:0;
}


// > ( n1 n2 -- fl )
void greater(void) {
  cell n2=POP; 
  TOS=(TOS>n2) ? -1:0;
}


// < ( n1 n2 -- fl )
void less(void) {
  cell n2=POP; 
  TOS=(TOS<n2) ? -1:0;
}


// >= ( n1 n2 -- fl )
void greaterequals(void) {
  cell n2=POP; 
  TOS=(TOS>=n2) ? -1:0;
}


// <= ( n1 n2 -- fl )
void lessequals(void) {
  cell n2=POP;	
  TOS=(TOS<=n2) ? -1:0;
}


// 0= ( x1 -- fl )
void zeroequals(void) {
  TOS=(TOS==0) ? -1:0;
}


// 0< ( n1 -- fl )
void zeroless(void) {
  TOS=(TOS<0) ? -1:0;
}


// 0> ( x1 -- fl )
void zerogreater(void) {
  TOS=(TOS>0) ? -1:0;
}


// u> ( u1 u2 -- fl )
void ugreater(void) {
  ucell u2=POP; 
  TOS=((ucell)TOS>u2) ? -1:0;
}


// u< ( u1 u2 -- fl )
void uless(void) {
  ucell u2=POP;	
  TOS=((ucell)TOS<u2) ? -1:0;
}


// u>= ( u1 u2 -- fl )
void ugreaterequals(void) {
  ucell u2=POP; 
  TOS=((ucell)TOS>=u2) ? -1:0;
}


// u<= ( u1 u2 -- fl )
void ulessequals(void) {
  ucell u2=POP; 
  TOS=((ucell)TOS<=u2) ? -1:0;
}


// d= ( d1 d2 -- fl )
void dequals(void) {
	dcell d1;
        dcell d2;
	DPOP(d2); 
        DPOP(d1);
	TOS=(d1==d2) ? -1:0;
}


// d<> ( d1 d2 -- fl )
void dnotequals(void) {
	dcell d1;
        dcell d2;
	DPOP(d2); 
        DPOP(d1);
	TOS=(d1!=d2) ? -1:0;
}


// d> ( d1 d2 -- fl )
void dgreater(void) {
	dcell d1;
        dcell d2;
	DPOP(d2); 
        DPOP(d1);
	TOS=(d1>d2) ? -1:0;
}


// d< ( d1 d2 -- fl )
void dless(void) {
	dcell d1;
        dcell d2;
	DPOP(d2); 
        DPOP(d1);
	TOS=(d1<d2) ? -1:0;
}


// within ( u min max -- fl )
void within(void) { // min <= u <max
	ucell u2=POP;
        ucell u1=POP;
        ucell u=TOS;
	TOS=((u-u1)<(u2-u1)) ? -1:0;
}


// ********** HIGH FORTH **********

// ********** VARIABLE **********


// tib ( -- addr )
void tib(void) {
  PUSH((ucell)vTib);
}


// #tib ( -- addr )
void sharptib(void) {
  PUSH((ucell)&vSharpTib);
}


// >in ( -- addr )
void gin(void) {
  PUSH((ucell)&vIN);
}


// base ( -- addr )
void base(void) {
  PUSH((ucell)&vBase); 
  vBase=(vBase<2) ? 2:vBase; 
  vBase=(vBase>32) ? 32:vBase;
}


// state ( -- addr )
void state(void) {
  PUSH((ucell)&vState);
}


// pad ( -- addr )
void pad(void) {
  PUSH((ucell)vPad);
}



// bl ( -- n )
void blf(void) {
  PUSH(Spc);
}


// ********** CTRLFLOW **********


// <mark ( -- addr )
void lmark(void) {
  here(); 
  TOS-=cellsize;
}


// <resolve ( addr -- )
void lresolve(void) {
  comma();
}


// >mark ( -- addr )
void gmark(void) {
  here(); 
  vHere+=cellsize;
}


// >resolve ( addr -- )
void gresolve(void) {
  here(); 
  TOS-=cellsize; 
  swap(); 
  store();
}


// do ( -- addr )
void dof(void) {
  CompileCxt(iDODO); 
  gmark(); 
  lmark();
}


// ?do ( -- addr )
void isdof(void) {
  CompileCxt(iISDO); 
  gmark(); 
  lmark();
}


// loop ( addr -- )
void loopf(void) {
  CompileCxt(iLOOP); 
  lresolve(); 
  gresolve();
}


// +loop ( addr -- )
void plusloop(void) {
  CompileCxt(iPLOOP); 
  lresolve(); 
  gresolve();
}


// if ( -- addr )
void iff(void) {
  CompileCxt(iDOCBR); 
  gmark();
}


// then ( addr -- )
void thenf(void) {
  gresolve();
}


// else ( addr1 -- addr2 )
void elsef(void) {
  CompileCxt(iDOBR); 
  gmark(); 
  swap(); 
  gresolve();
}


// begin ( -- addr )
void beginf(void) {
  lmark();
}


// while ( dest -- orig dest )
void whilef(void) {
  CompileCxt(iDOCBR); 
  gmark(); 
  swap();
}


// until ( addr -- )
void untilf(void) {
  CompileCxt(iDOCBR); 
  lresolve();
}


// repeat ( addr1 -- addr2 )
void repeatf(void) {
  CompileCxt(iDOBR); 
  lresolve(); 
  gresolve();
}


// again ( addr -- )
void againf(void) {
  CompileCxt(iDOBR); 
  lresolve();
}


// leave ( -- )
void leavef(void) {
  pRS-=2; 
  EXIT;
}


// unloop ( -- )
void unloopf(void) {
  pRS-=3;
}


// recurse ( -- )
void recursef(void) {
  linkg(); 
  comma();
}


// case ( -- 0 )
void casef(void) {PUSH(0);}


// of ( #of -- orig #of+1 / x -- )
void caseof(void) {
	TOS++; 
        tor();
	CompileCcon(&xt_over);
	CompileCcon(&xt_equal);
	PUSH((ucell)&xt_if); 
        executew();
	CompileCcon(&xt_drop);
	rfrom();
}


// endof ( orig1 #of -- orig2 #of )
void endof(void) {
	tor(); 
        PUSH((ucell)&xt_else); 
        executew(); 
        rfrom();
}


// endcase ( orig 1..orign #of -- )
void endcase(void) {
	ucell i;
        ucell u=POP;
	CompileCcon(&xt_drop);
	if (u>0) {
          for (i=0; i<u; i++) {
            PUSH((ucell)&xt_then); 
            executew();
          }
        }
}


// abort ( -- )
void abortf(void) {
#ifdef WITH_ISR
        isrdisable();
#endif        
#ifdef WITH_LCD
  lcd_off();
#endif // #ifdef WITH_LCD
  pDS=pDSzero; 
  pRS=pRSzero; 
  vIN=0; 
  vSharpTib=0; 
  vState=0;
}


// abort" ( x -- )
void aborts(void) {
  iff(); 
  dotstring(); 
  CompileCcon(&xt_abort); 
  thenf();
}



// ********** Interpreter **********


// [ ( -- )
void lbracket(void) {
  vState=0;
}  // Interprete


// ] ( -- )
void rbracket(void) {
  vState=1;
}  // Compile


// bin ( -- )
void binf(void) {
  vBase=2;
}


// decimal ( -- )
void decimal(void) {
  vBase=10;
}


// hex ( -- )
void hexf(void) {
  vBase=16;
}



// accept ( addr n1 -- n2 )
void accept(void) { // read until A,D,AD,DA
	char fRun=1;
        char k;
        char *pTib;
        char maxnum;
        char num=0;
	
	maxnum=POP; 
        pTib=(char *)POP;
	while (fRun) {
	  key(); 
          k=POP;
	  if (((k==BackSpc1) || (k==BackSpc2))&&(num>0))  {
            num--; 
            *--pTib=Spc; 
            PUSH(k); 
            emit();
            blf();
            emit();
            PUSH(k); 
            emit();
          }
	  if ((k>=Spc)&&(num<maxnum)) {
            num++; 
            *pTib++=k; 
            PUSH(k); 
            emit();
          }
//	  if ((k==CRD)||(k==CRA)) {
	  if ((k==CRD)) {
            fRun=0; 
//!!!            crf();
            blf();
            emit();
          }
	  if (num>=maxnum) {
            fRun=0;
          }
	}
	PUSH(num); 
}


// refill ( -- f )
void refill(void) {
  tib(); 
  PUSH(tibsize); 
  accept(); 
  vSharpTib=POP; 
  vIN=0; 
  PUSH(-1);
}


// source ( -- addr u )
void source(void) {
  tib(); 
  PUSH(vSharpTib);
}


// count ( addr -- addr+1 u )
void count(void) {
  dup(); 
  cfetch(); 
  TOSi(1)++;
}


// word ( c -- addr )
void wordf(void) {
	unsigned char c=POP, i=1;

	if (vSharpTib) {
	  while((vTib[vIN]==c)&&(vIN<vSharpTib)) {
            vIN++;
          }  // Skip spaces
	  while((vTib[vIN]!=c)&&(vIN<vSharpTib)) {          // Until c
		vPad[i]=vTib[vIN]; 
                i++; 
                vIN++;  // Copy to PAD
	  }
	  if((vTib[vIN]==c)&&(vIN<vSharpTib)) {
            vIN++;
          }  // Behind c delimiter 
	}
	vPad[0]=--i;  // Length
	PUSH((ucell)vPad);
}


// parse ( c -- addr u )
void parse(void) {
	unsigned char c=POP, i=0;

	if (vSharpTib) {
	  PUSH((ucell)&vTib[vIN]);         // Addr
	  while((vTib[vIN]!=c)&&(vIN<vSharpTib)) {
            i++; 
            vIN++;
          }  // Until c
	  PUSH((ucell)i);                  // Length
	  if ((vTib[vIN]==c)&&(vIN<vSharpTib)) {
            vIN++;
          }         // Behind c delimiter
	}
}


// number ( addr -- n )
void number(void)
{
	char *pStart, *pEnd;
	ucell x;
        ucell y=0;
        ucell len;

	count(); 
        len=POP; 
        pStart=(char *)POP;
	*(pStart+len)=0;  // Write end of number!
	if (*pStart=='-') {
          pStart++; 
          len--; 
          y=1;
        }
	base(); 
        drop();
	if (len) {
	  x=strtoul(pStart, &pEnd, vBase);
	  if (y) {
            x=-x;
          }
	  if (((ucell)pStart+len)==(ucell)pEnd) {
            PUSH(x);
          } else {
            vErrors|=2; 
            abortf();
          }
	} else {
          vErrors|=1;
        }
}


// find ( addr -- addr 0 | xt +-1 )
void find(void)
{
	short int i=PrimLast, j, len;
	char *p1=(char *)POP, *pbak=p1, *p2;
	ucell *Link, Linkbak, k=0;

	len=*p1 & 0x1F;
	if (vHead) {
          Link=(ucell *)vHead; 
          k=1;
        }  // Link to new word
	while (k) {  // Forth words
	  if ( (((*Link>>24)&0x1F)==len) && ((((*Link>>24)&sm))) ) {   // Compare length & check smudge
		  Linkbak=(ucell)Link+cellsize;        // Begin of name
		  j=len; 
                  p1=pbak; 
                  p2=(char *)Linkbak;
		  while((j>0)&&(*++p1==*p2++)) {
                    j--;
                  }  // Compare text
		  if (!j) {
			Linkbak+=len;
			if (Linkbak&3) {
                          Linkbak=(Linkbak&~3)+cellsize;
                        }		// Align
			PUSH(Linkbak);	// XT
			if ((*Link>>24)&im) {
                          PUSH(1);
                        } else {
                          PUSH(-1);
                        }		// 1 Immed
			k=0; i=-1;	// True
		  }
	  }
	  if (k) {
            Linkbak=*Link&0x7FFFFF;
          } else {
            Linkbak=0;
          }		// Last,zero?
	  if (!Linkbak) {
            k=0;
          }
	  if (k) {
                Linkbak+=AddrRAM<<24;
		Link=(ucell *)Linkbak;
	  }
	}

	  while(i>=0) {
		if ((primwords[i].wlen&0x1F)==len) {     // Compare length
		  j=len; 
                  p1=pbak; 
                  p2=primwords[i].wname;
		  while((j>0)&&(*++p1==*p2++)) {
                    j--;
                  }  // Compare text
		  if (!j) {
			PUSH((ucell)&primwords[i].wcall);  // XT
			if (primwords[i].wlen&im) {
                          PUSH(1);
                        } else {
                          PUSH(-1);
                        }
			i=-1;
		  }
		}
		if (i==0) {
                  PUSH((ucell)pbak); 
                  PUSH(0);
                } // False
		i--; 
	  }
}


// ********** Compiler **********


// link> ( -- xt )
void linkg(void)
{
	ucell Linkbak=cellsize+(ucell)vHead;
	Linkbak+=(*(ucell *)vHead>>24)&0x1F;			// Head+len
	if (Linkbak&3) {
          Linkbak=(Linkbak&~3)+cellsize;
        }	// Align
	PUSH(Linkbak);  // lfa->cfa
}


// >body ( xt -- addr )
void gbody(void) {
  TOS+=cellsize;
}


// ascii ( -- c )
void ascii(void)
{
	blf(); 
        wordf(); 
        TOS++; 
        TOS=*(unsigned char *)TOS;	// c to stack
	if (vState) {
           CompileCxt(iDOLIT); 
           comma();
         }	// c to Here
}


// alignhere ( -- )
void alignhere(void)
{
	if ((ucell)vHere&3) {
          vHere=(char *)(((ucell)vHere&~3)+cellsize);
        }
}

// align ( u -- u )
void align(void)
{
	if (TOS & 3 ) {
          TOS=(((ucell)TOS & ~3)+cellsize);
        }
}


// allot ( n -- )
void allot(void) {
  vHere+=POP;
}


// , ( x -- )
void comma(void)
{
//	*(ucell *)vHere=POP; vHere+=cellsize;
	ucell x=POP;
        *(ucell *)vHere=x; 
        vHere+=cellsize;
}


// c, ( c -- )
void ccomma(void)
{
//	*vHere++=POP;
	char c=POP;
        *vHere++=c;
}


// w, ( x -- )
void wcomma(void)
{
//	*(WORD *)vHere=POP; vHere+=2;
	WORD w=POP;
        *(WORD *)vHere=w; 
        vHere+=2;
}


// s, ( addr u -- )
void scomma(void)
{
	unsigned char len=POP;
	char *addr1=(char *)POP;
	PUSH(len); 
        ccomma();
	while (len>0) {
          PUSH(*addr1++); 
          ccomma(); 
          len--;
        }
}


// compile ( -- )
void compile(void) {
	PC+=cellsize; 
        PUSH(PC); 
        fetch(); 
        comma(); 
}


// [compile] ( -- )
void bracketcompile(void) {
	tick(); 
        comma();
}


// ' ( -- xt )
void tick(void) {
	blf(); 
        wordf(); 
        find();
	if (!POP) {
          vErrors|=0x20; 
          abortf();
        }
}


// ['] ( -- )
void brackettick(void) {
	CompileCxt(iDOLIT); 
        tick(); 
        comma();
}


// postpone ( -- )
void postpone(void) {
	blf(); 
        wordf(); 
        find(); 
	if ((cell)TOS<0) {
          pDS--; 
          CompileCcon(&xt_compile); 
          comma(); 
          return;
        } // noimmed
	if ((cell)TOS)   {
          pDS--; 
          comma(); 
          return;
        }  // [compile] immed
	pDS-=2; 
       vErrors|=0x20; 
       abortf();
}

void smudge(void) {
  if (vHead) {
    char *pLen=vHead+3;
    *pLen^=sm;
  }
}

// (create) ( name -- )
void docreate(void) {						// |Flg+Len|Link|Name Align| 
	char *bakH;
	unsigned char Len;
	ucell Link;

	blf(); 
        wordf(); 
        count(); 
        Len=TOS;  // ( -- addr n )
	if (Len>0) {
	  bakH=vHead; 
          vHead=vHere;			// Update Head
	  PUSH((ucell)bakH); 
          wcomma();		// Link low 16b
	  Link=((ucell)bakH>>16)&0x7F;		// Link hi 23b 
	  Link|=0x80;  // ROM/RAM
	  PUSH(Link); 
          ccomma(); 
          scomma();	// Link hi, Length+Name
	  alignhere();							// 4B align
          smudge();
	} else {
          twodrop();
        }
}


// create ( name -- )
void createf(void) {						// |Flg+Len|Link|Name Align|(con)|PFA| 
	docreate(); 
        CompileCpfa(iDOCON);	// comma PFA docon
	PUSH((ucell)vHere+cellsize); 
        comma(); // comma PFA new word
}


// <builds ( name -- )
void builds(void) {						// |Flg+Len|Link|Name Align|-1|-1|
	docreate();
//        smudge(); // !!!
	vHere+=cellsize; 
        vHere+=cellsize;	// Cells for Flash overwrite
}


// does> ( -- )
void does(void) {
	char *bak=vHere;
	linkg(); 
        herewrite(); 
        CompileCpfa(iDODOES);	// Overwrite xt by dodoes
	PUSH(PC); 
        comma();							// Overwrite xt+1 by PC does>
	vHere=bak; 
        EXIT;
}


// : ( -- )
void colon(void) {
  if ( ! vState ) {
	docreate(); 
        smudge(); // invalidate currently definied word
        CompileCpfa(iENTER); 
        /* PFA docolon */ 
        rbracket();
  } else {
    vErrors|=0x80;
    abortf();
  }
}


// ; ( -- )
void semicolon(void) {
        if (vState) {
          smudge(); // validate the word
          CompileCxt(iEXIT); 
          /* doexit */ 
          lbracket();
        } else {
          vErrors|=0x40;
          abortf();
        }
}


// immediate ( -- )
void immediate(void) {
	char *pLen=vHead+3;
	*pLen|=im;
}


// s" ( -- addr n)
void squote(void) {
	PUSH('"'); 
        parse();
	if (vState) {
          CompileCxt(iDOSLIT); 
          scomma(); 
          alignhere();
        }
}


// literal ( x -- )
void literal(void) {
	CompileCxt(iDOLIT); /* doliteral */ 
        comma();
}


// constant ( x -- )
void constant(void) {
	docreate(); 
       CompileCpfa(iDOCON); /* PFA doconstant */ 
       comma();
}


// variable ( -- ) 
void variable(void) {
	docreate(); 
        CompileCpfa(iDOVAR); /* PFA dovariable */
	vHere+=cellsize;
}




// (to) ( x -- )
void dotof(void) {
	PC+=cellsize; 
        pDATA PC=POP;
}


// to ( x name -- ) 
void tof(void) {
	tick(); 
        TOS+=cellsize; 
        fetch();
	if(vState) {
          CompileCcon(&xt_doto); 
          comma();
        } else {
          store();
        }
}


// defer ( name -- )
void defer(void) {
	docreate(); 
        CompileCpfa(iDODEF); /* PFA dodefer */

        CompileCxt(iNOP);
        
//	here(); comma();
//        vHere+=cellsize;
}


// defer@ ( xt1 -- xt2 )
void deferfetch(void) {
//	TOS+=cellsize; TOS=pDATA(pDATA(TOS));
	TOS+=cellsize; TOS=pDATA(TOS);
}


/*
 * Do not use the words from base dictionary (primitives in C) !
 */
// defer! ( xt2 xt1 -- )
void deferstore(void) {
	ucell x1=POP+cellsize;
//	pDATA(pDATA(x1))=POP;
        pDATA(x1) = POP;
}






// interpret ( ? -- ? )
void interpret(void)
{
	char fRun=1;
	
	vErrors=0;
	while (fRun) {	
	  blf(); 
          wordf(); 
          dup();
	  cfetch();
	  if (!POP) {
            drop(); 
            vErrors|=1;
          } else { // Empty TIB
            find();	
            isdup();
	    zeroequals();
	    if (POP) {
              number(); 
              if (vState) {
                CompileCxt(iDOLIT); 
                comma();
              }
            } else {  // compile number
              zerogreater();
	      if (POP) {
                executew();
              }  else { // immediate
                if (vState) {
                  comma();
                } else {
                  executew();
                }  // compile/interpreter
              }
            }
	  }
	  if (vErrors) {
            fRun=0;
          }
	  if (vState==EndState) {
            fRun=0;
          }
	}
}


// quit ( -- )
void quit(void)
{
	pDS=pDSzero; 
        pRS=pRSzero; 
        lbracket();
	do {
	  if (!vState) {
            crf(); 
            if (vBase == 10 ) {
              f_puts("$>");
            } else if (vBase == 16) {
              f_puts("#>");
            } else {
              f_puts(".>");
            }
          };
	  refill(); 
//          f_puts(" ");  // !!!
          POP;
	  vErrors=0; 
          interpret();
	  if (vErrors<=1) {
            if (!vState) {
              f_puts(" ok");
            } else {
              crf();
            }
          } else {
            if (vErrors & 0x20) {
              f_puts(" ??? Word not found!");
            } else if (vErrors & 0x08) {
              f_puts(" ??? Divide by zero!");
            } else if (vErrors & 0x04) {
              f_puts(" ??? Stack underflow!");
            } else if (vErrors & 0x02) {
              f_puts(" ??? Invalid input!");
            } else if (vErrors & 0x40) {
              f_puts(" ??? Only compile mode!");
            } else if (vErrors & 0x80) {
              f_puts(" ??? Only interpreter mode!");
            } else {
              f_puts(" ??? Unknown error code!");
            }
          }
//          f_puts("\n");
	} while (vState!=EndState);
}



// ********** EMIT **********


// cr ( -- )
void crf(void) {
  PUSH(CRD); 
  emit(); 
  PUSH(CRA); 
  emit();
}


// space ( -- )
void spacef(void) {
  blf(); 
  emit();
}


// spaces ( n -- )
void spaces(void) {
  cell i, n=POP; 
  if (n>0) {
    for(i=0; i<n; i++) {
      blf(); 
      emit();
    }
  }
}


// type ( addr u -- )
void typef(void) {
	ucell i, u=POP;
	char *p=(char *)(ucell)POP;
	if (u>0) {
          for(i=0; i<u; i++) {
            PUSH(*p++); 
            emit();
          }
        }
}


// ." ( -- )
void dotstring(void) {
  squote(); 
  if (vState) {
    CompileCcon(&xt_type);
  } else {
    typef();
  }
}


// .( ( -- )
void dotlparen(void) {
  PUSH(')'); 
  parse(); 
  typef();
}


// ( ( -- )
void lparen(void) {
  PUSH(')'); 
  parse(); 
  twodrop();
}


// hold ( c -- )
void hold(void) {
	cell c=POP, len=vPad[0], i;
	char *p1, *p2;

	if (len<padsize-1) {
	  len++; 
          vPad[0]=len; 
          p1=vPad+len; 
          p2=p1++;
	  for (i=0; i<len; i++) {
            *p1--=*p2--;
          }
	  //memmove(&vPad[1], &vPad[2], vPad[0]);
	}
	vPad[1]=c;
}


// <# ( -- )
void sharpl(void) {
  vPad[0]=0;
}


// # ( x1 -- x2 )
void sharp(void) {
	cell x;
	base(); 
        cfetch(); 
        udivmod(); 
        swap(); 
        x=POP;
	if (x>9) {
          x+=7;
        }
	PUSH(x+0x30); 
        hold();
}


// #s ( x -- 0 )
void sharps(void) {
  do{
    sharp();
  } while (TOS!=0);
}


// #> ( x -- addr len )
void sharpg(void) {
  TOS=(ucell)vPad; 
  count();
}


// u.r ( u n -- )
void udotr(void) {
	cell n=POP, len;
	sharpl(); 
        sharps(); 
        sharpg(); 
        len=TOS; // ( u -- addr len )
	PUSH(n-len); 
        spaces(); 
        typef();
}


// u. ( u -- )
void udot(void) {
  PUSH(0); 
  udotr(); 
  blf(); 
  emit();
}


// .r ( n1 n2 -- )
void dotr(void) {
	cell n2=POP, n1=TOS, len;
	absf(); 
        sharpl(); 
        sharps(); 
        if (n1<0) {
          PUSH('-'); 
          hold();
        }
	sharpg(); 
        len=TOS; // ( u -- addr len )
	PUSH(n2-len); 
        spaces(); 
        typef();
}

// h. ( n --- )
void hdot(void) {
  int basebak = vBase;
  vBase = 16;
  udot();
  vBase = basebak;  
}

// d. ( n --- )
void ddot(void) {
  int basebak = vBase;
  vBase = 10;
  dot();
  vBase = basebak;  
}

// . ( n -- )
void dot(void) {
  PUSH(0); 
  dotr(); 
  blf(); 
  emit();
}


// \ ( -- )
void backslash(void) {
  vIN=vSharpTib;
}



// .s ( -- )
void dots(void) {
	cell i, n=DScnt;
	ucell *p=(ucell *)pDSzero;

	if (n>0) {
        for(i=0; i<n; i++) {
	  PUSH(*++p);
	  if (vBase==10) {
            dot();
          } else {
            udot();
          }
	  blf(); 
          emit();
        }
      }
//      crf();
}


// ver ( -- )
void ver(void) {
  f_puts((char*)StrVer);
  f_puts("\n");
}


// ********** VOCABULARY **********


// words ( -- )
void wordsf(void)
{
	short int i=PrimLast, j, len;
	char  *p;
	ucell *Link, Linkbak, k=0;

	if (vHead) {
          Link=(ucell *)vHead; 
          k=1;
        }	// Link to last word
        crf();
	while (k) {								// Forth words
	  len=(short int)(*Link>>24)&0x1F ;
	  if (len) {								// Length?
		  Linkbak=(ucell)Link+cellsize;		// Begin of name
		  j=len; 
                  p=(char *)Linkbak;
		  for(j=0; j<len; j++) {
                    PUSH(*p++); 
                    emit();
                  }
		  blf(); 
                  emit();
	  }
	  if (k) {
            Linkbak=*Link&0x7FFFFF;
          } else {
            Linkbak=0;
          }	// Last,zero?
	  if (!Linkbak) {
            k=0;
          }
	  if (k) {
                Linkbak+=AddrRAM<<24;
		Link=(ucell *)Linkbak;
	  }
	}

	if (primwords[i].wlen&pr) { // Primitives?
	 while(i>=0) {
	   if (primwords[i].wlen&0x1F) {  // Length?
                f_puts(primwords[i].wname);
		blf(); 
                emit();
	   }
	 i--;
         }
       } 
       crf();
}


// ********** DEVICE **********


// coretim ( -- u )
void coretim(void) {
  PUSH(ReadCoreTimer());
}




// ChipKit specific
// restart MCU to bottloader
// ( f --- )
void reset(void) {
  UINT bootloader = POP;
  executeSoftReset(bootloader);
}


#ifdef WITH_EEPROM
// *******************************************************
// **** EEPROM access ************************************
// *******************************************************
typedef  union  {
    uint32_t i;
    uint8_t b[4];
  } ebytes_t;

void EE_WR_Word(uint32_t address, uint32_t data) {
  ebytes_t ebytes;
  ebytes.i = data;
  EEPROM.write(address++, ebytes.b[0]);
  EEPROM.write(address++, ebytes.b[1]);
  EEPROM.write(address++, ebytes.b[2]);
  EEPROM.write(address++, ebytes.b[3]);
}

uint32_t EE_RD_Word(uint32_t address) {
  ebytes_t ebytes;
  ebytes.b[0] = EEPROM.read(address++);
  ebytes.b[1] = EEPROM.read(address++);
  ebytes.b[2] = EEPROM.read(address++);
  ebytes.b[3] = EEPROM.read(address++);
  return ebytes.i;
}


// emptyeeprom
void FEraseEEPROM(void) {
  EEPROM.clear();
}

// e!
void FEEPROMStore(void) {
  uint32_t addr;
  uint32_t data;
  addr = POP;
  data = POP;
  EE_WR_Word(addr*cellsize, data);
}


// e@
void FEEPROMFetch(void) {
  uint32_t addr;
  addr = POP;
  PUSH(EE_RD_Word(addr*cellsize));
}

#endif


// *******************************************************
// ********** Save/restore system to/from FLASH **********
// *******************************************************

// This technique prevent to found MAGIC string in program area.
uint32_t getmagic(uint8_t index) {  // Nif4|rB6
  switch (index) {
    case 0:
      return 0x4e4e4e4e;
      break;
    case 1:
      return 0x69696969;
      break;
    case 2:
      return 0x66666666;
      break;
    case 3:
      return 0x34343434;
      break;
    case 4:
      return 0x7c7c7c7c;
      break;
    case 5:
      return 0x72727272;
      break;
    case 6:
      return 0x42424242;
      break;
    case 7:
      return 0x36363636;
      break;
    default:
      return 0;
      break;
  }
}

// Uniform address into KSEG0 area
uint8_t *maskaddr(uint8_t *addr) {
  uint32_t tmp;
  tmp = (uint32_t)addr;
  tmp &= ~0x20000000;
  return (uint8_t*)tmp;
}

// erase free usable flash
void eraseflash(void) {
  uint8_t *from;
  uint8_t *to;
  from = pagealign((uint8_t *)FREE_FLASH_START);
  to = pagealign((uint8_t *)FREE_FLASH_END);
  uint8_t *i;
  f_puts("Erasing ");
  for (i=from;i<to; i+= PAGE_SIZE) {
    f_putc('.');
    NVMerase((UINT*)i);
  }
  f_puts("\n");
}

// Find the occurence of MAGIC from the beginning of the flash to end of flash
// return the address of found MAGIC
uint8_t * findmagic(int occurence) {
  uint8_t *addr;
  int mi;
  uint8_t c;
  mi = 0;
  addr = pagealign((uint8_t *)FREE_FLASH_START);
  while (1) {
    c = *addr;
    if (c == (getmagic(mi) & 0x000000ff)) {
      ++mi;
      if (mi == 8) {
        --occurence;
        if (occurence == 0) {
          return maskaddr(addr-7);
        } else {
          mi = 0;
        }
      }
    } else {
      mi=0;
    }
    ++addr;
    if (addr>=(uint8_t*)FREE_FLASH_END) {
      return 0;
    }
  }
  return 0;
}

// Find the last occurence of MAGIC from the beginning of the flash to end of flash
// return the address of found MAGIC
uint8_t *findLastMagic(void) {
  uint8_t *magic;
  uint8_t *prevmagic;
  int i;
  
  prevmagic = NULL;
  i = 1;
  
  magic = findmagic(i);
  while (magic) {
    prevmagic = magic;
    ++i;
    magic = findmagic(i);
  }
  return prevmagic;
}

// align address to page boundary
uint8_t *pagealign(uint8_t *addr) {
  if (addr != (uint8_t*)(StartOfFlashPage((uint32_t)addr))) {
    addr = (uint8_t*)(StartOfFlashPage((uint32_t)addr) + BYTE_PAGE_SIZE);
  } else {
    addr = (uint8_t*)(StartOfFlashPage((uint32_t)addr));
  }
  return addr;
}

// Megkeresi az első, a paraméterben megadott méretű szabad FLASH területet elejét.
// Egy terület akkor szabad, ha csupa 0xff-et tartalmaz.
// A terület vége a linker fájlban meghatározott EEPROM terület eleje-1. (EEPROM_START)
uint8_t *_findFreeFlash(uint32_t bsize) {
  uint8_t *magic;
  uint8_t *addr;
  uint8_t *b;
  uint8_t *e;
  uint8_t *maxaddr;
  uint8_t c;
  int i;
  addr = maskaddr((uint8_t*)FREE_FLASH_START);
  maxaddr = maskaddr((uint8_t*)(FREE_FLASH_END));
  i = 1;
  magic = maskaddr(findLastMagic());
  if (magic>addr) {
    addr = magic+8;  // MAGIC átlépése
  } else {
    addr = pagealign(addr);    // biztonsági tartalék, nehogy a kódterületre írjon
  }
  b = addr;
  e = b;
  while ( (((uint32_t)addr < (uint32_t)maxaddr)) && ((e-b)<bsize)) {
    c = *addr;
      ++addr;
    if (c == 0xff ) {
      e = addr;
    } else {
      b = addr;
      e = addr;
    }
  }
  if ((e-b)<bsize) {
    return NULL;
  } else {
    if (! magic) {
      return pagealign(b);
    } else {
      return b;
    }
  }
}




// Megkeresi a paraméterben megadott méretű szabad FLASH területet elejét.
// Egy terület akkor szabad, ha csupa 0xff-et tartalmaz.
// A terület vége a linker fájlban meghatározott EEPROM terület eleje-1. (EEPROM_START)
// Ha nem talál elegendő szabad flash-t, de talál MAGIC-al jelölt területet, akkor "eraseflash"-al töröl,
// és újrakezdi a keresést.
uint8_t *findFreeFlash(uint32_t bsize) {
  uint8_t *addr;
  uint8_t *maxaddr;
  uint32_t tmp;
  char x;
  addr = maskaddr(_findFreeFlash(bsize));
  maxaddr = maskaddr((uint8_t*)(FREE_FLASH_START));
  if (addr) {
    return addr;
  }
  f_puts("No more free flash space!\n");
  addr = pagealign(findmagic(1));
  addr = pagealign(addr);
  eraseflash();
  return maskaddr(_findFreeFlash(bsize));
}



// Megkeresi a szabad flash terület elejét.
// Ha ez nem MAGIC-al kezdődik, akkor megjelöli az elejét MAGIC-al.
// sysvars-t feltölti az aktuális értékekkel.
// 
void syssave(void) {
  uint32_t i;
  UINT *addr;
  UINT *dictptr;
  uint8_t *magic;
  uint8_t *fp;
  uint8_t *rp;
  uint32_t *psysvars;

  for(i=0;i<8;++i) {
    sysvars.magic[i] = getmagic(i);
  }
  sysvars.here = vHere;
  sysvars.head = vHead;
  if (vHere > vHead) {
    sysvars.savedbytes = vHere-vDict;
  } else {
    sysvars.savedbytes = vHead-vDict;
  }
  f_puts("Saving current system (");
  f_putdec(sysvars.savedbytes);
  f_puts(" bytes)\n");
  addr = (UINT*)findFreeFlash(sysvars.savedbytes + sizeof(sysvars));
  if (! addr) {
    f_puts("Not found available enough FLASH to save system !");
    return;
  }
// save sysvars
  psysvars = (uint32_t*)&sysvars;
  for(i=0; i<sizeof(sysvars);i+=sizeof(uint32_t)) {
    NVMwrite(addr, *psysvars);
    ++addr;
    ++psysvars;
  }
  f_putc(',');
// save dictionary
  dictptr = (UINT*)vDict;
  for(i=0; i<sysvars.savedbytes;i+=sizeof(UINT)) {
    if ((i % 1024) == 0) {
      f_putc('.');
    }
    NVMwrite(addr, *dictptr);
    ++addr;
    ++dictptr;
  }
  f_puts("Done.\n");
  f_puts("Verifying saved system\n");
  fp = findLastMagic();
  if (!fp) {
    f_puts("Not found saved system header !\n");
    return;
  }
// verify sysvars  
  rp = (uint8_t*)&sysvars;
  for(i=0; i<sizeof(sysvars);i+=sizeof(uint8_t)) {
    if (*rp != *fp) {
//!!!      f_printf("Verify error RAM: %x FLASH: %x --> %x!= %x !\n", rp, fp, *rp, *fp);
    }
    ++rp;
    ++fp;
  }
  f_putc(',');
// verify dictionary  
  rp = (uint8_t*)vDict;;
  for(i=0;i<sysvars.savedbytes;++i) {
    if (*rp != *fp) {
//!!!      f_printf("Verify error RAM: %x FLASH: %x --> %x!= %x !\n", rp, fp, *rp, *fp);
    }
    if ((i % 1024) == 0) {
      f_putc('.');
    }
    ++rp;
    ++fp;
  }
  f_puts("Done.\n");
}

// Flash-ban MAGIC-ot keres.
// Ha nem talál, akkor nem csinál semmit.
// Ha megtalálta, akkor MAGIC+8 címről feltölti sysvars-t.
// sysvars alapján másolás FLASH->RAM.
// sysvars-ból feltölti a rendszerváltozókat.
// ha bootword != 0, veremre teszi "bootword" tartamát -> executew()-el indítja a rendszert.
void sysrestore(void) {
  int i;
  uint8_t *p8;
  uint8_t *ps;
  uint8_t *magic;
  ps = (uint8_t*)&sysvars;
  magic = findLastMagic();
  if (! magic) {
    f_puts("Could not found prevoiusly saved system datas !\n");
    return;
  } else {
    f_puts("System restoring ");
  }
// restore sysvars  
  p8 = magic;
  for(i=0;i<sizeof(sysvars); ++i) {
    *ps=*p8;
    ++ps;
    ++p8;
  }
// restore dictionay  
  f_puts("(");
  f_putdec(sysvars.savedbytes);
  f_puts(" bytes) ...");
  ps = (uint8_t*)vDict;
  for(i=0;i<sysvars.savedbytes;++i) {
    *ps = *p8;
    ++ps;
    ++p8;
  }
//  memcpy(&sysvars, vDict, sizeof(sysvars));
  vHere = sysvars.here;
  vHead = sysvars.head;
  AddrRAM=(ucell)vDict>>24; 
  pDS=pDSzero; 
  pRS=pRSzero; 
  vIN=0; 
  vSharpTib=0; 
  vBase=DEFAULT_BASE; 
  vState=0; 
  vErrors=0;
  f_puts(" done.\n");
  if (sysvars.bootword) {
    f_puts("Starting bootword.\n");
    PUSH(sysvars.bootword);
    executew();
  }
}


// Hig level interfaces

// ( occurence --- addr|0 )
void Ffindmagic(void) {
  int occurence;
  occurence = POP;
  PUSH((int)findmagic(occurence));
}


void FFindFreeFlash(void) {
  UINT bsize;
  bsize = POP;
  PUSH((UINT)findFreeFlash(bsize));
}


void FfindLastMagic(void) {
  PUSH((UINT)findLastMagic());
}

void FfindFreeFlash(void) {
  uint32_t bsize;
  uint8_t *addr;
  bsize = POP;
  addr = findFreeFlash(bsize);
  PUSH((UINT)addr);
}
void F_findFreeFlash(void) {
  uint32_t bsize;
  uint8_t *addr;
  bsize = POP;
  addr = _findFreeFlash(bsize);
  PUSH((UINT)addr);
}

void FNVMErase(void) {
  UINT *addr;
  addr = (UINT*)POP;
  PUSH((UINT)NVMerase(addr));  
}

void FNVMWrite(void) {
  UINT *addr;
  UINT data;
  addr = (UINT*)POP;
  data = POP;
  PUSH((UINT)NVMwrite(addr, data));
}


// set sysvard.bootword
// ( --- a )
void bootword(void) {
  PUSH((UINT)&sysvars.bootword);
}

#ifdef WITH_EXCEPTION_HANDLING
  #ifdef WITH_EEPROM
    // ( --- code stat addr )
    void getExceptionInfo(void) {
      PUSH(EE_RD_Word(0));
      PUSH(EE_RD_Word(4));
      PUSH(EE_RD_Word(8));
      clearExceptionInfo();
    }

  
    // ( --- )
    void clearExceptionInfo(void) {
      EE_WR_Word(0, -1);
      EE_WR_Word(4, -1);
      EE_WR_Word(8, -1);
    }
  #endif
#endif

// ********
int bootkey(int times) {
  int i;
  int k;
  int c;
  for (i=times;i;--i) {
    f_puts("Do you have ");
    f_putdec(i);
    f_puts(" seconds to abort system restore with ESC!\n");
    c = Serial.read();
    if (c != -1) {
      break;
    }
    delay(1000);
  }
  return c==27;
}


// warm ( -- )
void warm(void) {
	AddrRAM=(ucell)vDict>>24; 
	pDS=pDSzero; 
        pRS=pRSzero; 
	vIN=0; 
        vSharpTib=0; 
        vBase=DEFAULT_BASE; 
        vState=0; 
        vErrors=0;
#ifdef WITH_ISR
        isrdisable();
#endif        
	ver();
	quit();
}



// return with free bytes of dictionary
void Ffree(void) {
  PUSH(dictsize - (vHere-vDict));
}

// fill the dictionary with 0xff and set the sysvars fields.
void emptyDict(void) {
#ifdef WITH_ISR
        isrdisable();
#endif     
  memset((char *)vDict, 0xff, dictsize);
  vHere = vDict + sizeof(sysvars);
  vHead = 0;
  sysvars.here = vHere;
  sysvars.head = vHead;
  sysvars.bootword = 0;
//  memcpy(vDict, &sysvars, sizeof(sysvars));
}

void Fempty(void) {
  extdict_loaded = 0;
  emptyDict();
  warm();
}

// cold ( -- )

void cold(void)
{
	// Default init
#ifdef WITH_ISR
        isrdisable();
#endif 
        extdict_loaded = 0;
	AddrRAM=(ucell)vDict>>24; 
	pDS=pDSzero; 
        pRS=pRSzero; 
	vIN=0; 
        vSharpTib=0; 
        vBase=DEFAULT_BASE; 
        vState=0; 
        vErrors=0;
	FindLastC(); 
        emptyDict();
        sysvars.bootword = NULL;
	ver();
        if (! bootkey(9)) {
          f_puts("Trying to restore last saved system.\n");
          sysrestore();
        }
	quit();
}




#ifdef WITH_FLASH_DEBUG
void flashstart(void) {
  PUSH((UINT)pagealign((uint8_t *)FREE_FLASH_START));
}
void flashend(void) {
  PUSH(FREE_FLASH_END);
}
#endif

// ********** DICTIONARY **********

const PRIMWORD primwords[] =
{	// VMcore
{0,pr|6,       "(exit)",     (void *)exitw}, 
{1,pr|7,       "execute",    (void *)executew}, 
{2,pr|0,       "",           (void *)nextw}, 
{3,pr|7,       "(colon)",    (void *)enterw},
{4,pr|5,       "(lit)",      (void *)dolitw}, 
{5,pr|6,       "(slit)",     (void *)doslitw}, 
{6,pr|5,       "(con)",      (void *)doconw}, 
{7,pr|5,       "(var)",      (void *)dovarw},
{8,pr|6,       "(does)",     (void *)dodoes}, 
{9,pr|9,       "(dodefer)",  (void *)dodefer}, 
{10,pr|4,      "(do)",       (void *)dodo}, 
{11,pr|5,      "(?do)",      (void *)doisdo},
{12,pr|6,      "(loop)",     (void *)doloop}, 
{13,pr|7,      "(+loop)",    (void *)doplusloop}, 
{14,pr|8,      "(branch)",   (void *)dobranch}, 
{15,pr|9,      "(?branch)",  (void *)docbranch},
{16,pr|5,      "(val)",      (void *)dovalw},
{17,pr|3,      "nop",        (void *) nop},
// Stack
{1,  pr|4,     "drop",       (void *) drop}, 
{2,  pr|5,     "2drop",      (void *) twodrop}, 
{3,  pr|3,     "dup",        (void *) dup}, 
{4,  pr|4,     "2dup",       (void *) twodup},
{5,  pr|4,     "?dup",       (void *) isdup}, 
{6,  pr|3,     "nip",        (void *) nip}, 
{7,  pr|4,     "over",       (void *) over}, 
{8,  pr|5,     "2over",      (void *) twoover},
{9,  pr|4,     "pick",       (void *) pick}, 
{10, pr|5,     "stick",      (void *) stick}, 
{11, pr|4,     "roll",       (void *) roll}, 
{12, pr|5,     "-roll",      (void *) minusroll},
{13, pr|3,     "rot",        (void *) rot}, 
{14, pr|4,     "-rot",       (void *) minusrot}, 
{15, pr|4,     "swap",       (void *) swap}, 
{16, pr|5,     "2swap",      (void *) twoswap},
{17, pr|2,     ">r",         (void *) tor}, 
{18, pr|2,     "r>",         (void *) rfrom}, 
{19, pr|2,     "r@",         (void *) rfetch}, 
{20, pr|5,     "depth",      (void *) depth},
{21, pr|6,     "depth!",     (void *) depthwrite}, 
{22, pr|6,     "rdepth",     (void *) rdepth}, 
{23, pr|7,     "rdepth!",    (void *) rdepthwrite},
{24, pr|3,     "sp@",        (void *) spfetch}, 
{25, pr|3,     "rp@",        (void *) rpfetch},
{24, pr|3,     "sp0",        (void *) spzero}, 
{25, pr|3,     "rp0",        (void *) rpzero},
// Other
{1,  pr|4,     "emit",       (void *) emit}, 
{3,  pr|3,     "key",        (void *) key}, 
{4,  pr|4,     "?key",       (void *) iskey},
{5,  pr|1,     "i",          (void *) loop_i}, 
{6,  pr|1,     "j",          (void *) loop_j},  
{7,  pr|1,     "k",          (void *) loop_k}, 
// Memory
{1,  pr|1,     "@",          (void *) fetch}, 
{2,  pr|2,     "c@",         (void *) cfetch}, 
{3,  pr|2,     "w@",         (void *) wfetch}, 
{4,  pr|1,     "!",          (void *) store},
{5,  pr|2,     "c!",         (void *) cstore}, 
{6,  pr|2,     "w!",         (void *) wstore}, 
{7,  pr|2,     "+!",         (void *) plusstore},
{8,  pr|4,     "fill",       (void *) fillf}, 
{9,  pr|4,     "move",       (void *) movef}, 
{10, pr|4,     "head",       (void *) head}, 
{10, pr|4,     "here",       (void *) here}, 
{11, pr|5,     "here!",      (void *) herewrite},
{3,  pr|9,     "alignhere",  (void *) alignhere}, 
{11, pr|5,     "align",      (void *) align},
// Arithmetic
{1,  pr|1,     "+",          (void *) plus}, 
{2,  pr|1,     "-",          (void *) minus}, 
{3,  pr|2,     "d+",         (void *) dplus}, 
{4,  pr|2,     "d-",         (void *) dminus},
{5,  pr|1,     "*",          (void *) mult}, 
{6,  pr|2,     "m*",         (void *) mmult}, 
{7,  pr|3,     "um*",        (void *) ummult}, 
{9,  pr|6,     "um/mod",     (void *) umdivmod}, 
{10, pr|5,     "m/mod",      (void *) mdivmod}, 
{11, pr|5,     "u/mod",      (void *) udivmod}, 
{12, pr|4,     "/mod",       (void *) divmod},
{13, pr|1,     "/",          (void *) divf}, 
{14, pr|3,     "mod",        (void *) modn}, 
{15, pr|3,     "u*/",        (void *) umuldiv}, 
{16, pr|2,     "*/",         (void *) muldiv},
{17, pr|3,     ">>a",        (void *) arshift}, 
{18, pr|2,     ">>",         (void *) rshift}, 
{19, pr|2,     "<<",         (void *) lshift}, 
{20, pr|2,     "2*",         (void *) twomul},
{21, pr|2,     "2/",         (void *) twodiv}, 
{22, pr|3,     "min",        (void *) minf}, 
{23, pr|3,     "max",        (void *) maxf}, 
{24, pr|3,     "abs",        (void *) absf},
{25, pr|2,     "1+",         (void *) incf}, 
{26, pr|2,     "1-",         (void *) decf}, 
{27, pr|6,     "negate",     (void *) negate}, 
{28, pr|6,     "invert",     (void *) invert},
{29, pr|3,     "and",        (void *) andf}, 
{30, pr|2,     "or",         (void *) orf}, 
{31, pr|3,     "xor",        (void *) xorf},
// Logic
{1,  pr|4,     "andl",       (void *) andl}, 
{2,  pr|3,     "orl",        (void *) orl}, 
{3,  pr|3,     "not",        (void *) notl}, 
{4,  pr|1,     "=",          (void *) equals},
{5,  pr|2,     "<>",         (void *) notequals}, 
{6,  pr|1,     ">",          (void *) greater}, 
{7,  pr|1,     "<",          (void *) less}, 
{8,  pr|2,     ">=",         (void *) greaterequals},
{9,  pr|2,     "<=",         (void *) lessequals}, 
{10, pr|2,     "0=",         (void *) zeroequals}, 
{11, pr|2,     "0<",         (void *) zeroless}, 
{12, pr|2,     "0>",         (void *) zerogreater},
{13, pr|2,     "u>",         (void *) ugreater}, 
{14, pr|2,     "u<",         (void *) uless}, 
{15, pr|3,     "u>=",        (void *) ugreaterequals}, 
{16, pr|3,     "u<=",        (void *) ulessequals},
{17, pr|2,     "d=",         (void *) dequals}, 
{18, pr|3,     "d<>",        (void *) dnotequals}, 
{19, pr|2,     "d>",         (void *) dgreater}, 
{20, pr|2,     "d<",         (void *) dless},
{21, pr|6,     "within",     (void *) within},
// High FORTH
// Variable
{1,  pr|2,     "bl",         (void *) blf}, 
{2,  pr|4,     "base",       (void *) base}, 
{3,  pr|3,     ">in",        (void *) gin}, 
{4,  pr|3,     "pad",        (void *) pad},
{5,  pr|5,     "state",      (void *) state}, 
{6,  pr|3,     "tib",        (void *) tib}, 
{7,  pr|4,     "#tib",       (void *) sharptib},
// CtrlFlow
{1,  pr|5,     "<mark",      (void *) lmark}, 
{2,  pr|8,     "<resolve",   (void *) lresolve}, 
{3,  pr|5,     ">mark",      (void *) gmark}, 
{4,  pr|8,     ">resolve",   (void *) gresolve},
{5,  pr|im|2,  "do",         (void *) dof}, 
{6,  pr|im|3,  "?do",        (void *) isdof}, 
{7,  pr|im|4,  "loop",       (void *) loopf}, 
{8,  pr|im|5,  "+loop",      (void *) plusloop},
{9,  pr|im|2,  "if",         (void *) iff}, 
{10, pr|im|4,  "then",       (void *) thenf}, 
{11, pr|im|4,  "else",       (void *) elsef}, 
{12, pr|im|5,  "begin",      (void *) beginf},
{13, pr|im|5,  "while",      (void *) whilef}, 
{14, pr|im|5,  "until",      (void *) untilf}, 
{15, pr|im|6,  "repeat",     (void *) repeatf}, 
{16, pr|im|5,  "again",      (void *) againf},
{17, pr|4,     "exit",       (void *) exitw}, 
{18, pr|im|7,  "recurse",    (void *) recursef}, 
{19, pr|5,     "leave",      (void *) leavef}, 
{20, pr|6,     "unloop",     (void *) unloopf},
{21, pr|im|4,  "case",       (void *) casef}, 
{22, pr|im|2,  "of",         (void *) caseof}, 
{23, pr|im|5,  "endof",      (void *) endof}, 
{24, pr|im|7,  "endcase",    (void *) endcase},
{25, pr|5,     "abort",      (void *) abortf}, 
{26, pr|im|6,  "abort\"",    (void *) aborts}, 
// Interpreter
{1,  pr|im|1,  "[",          (void *) lbracket}, 
{2,  pr|1,     "]",          (void *) rbracket}, 
{3,  pr|3,     "bin",        (void *) binf}, 
{4,  pr|7,     "decimal",    (void *) decimal},
{5,  pr|3,     "hex",        (void *) hexf}, 
{6,  pr|6,     "accept",     (void *) accept}, 
{7,  pr|5,     "count",      (void *) count}, 
{8,  pr|4,     "find",       (void *) find},
{9,  pr|6,     "number",     (void *) number}, 
{10, pr|5,     "parse",      (void *) parse}, 
{11, pr|4,     "word",       (void *) wordf}, 
{12, pr|9,     "interpret",  (void *) interpret},
{13, pr|6,     "refill",     (void *) refill}, 
{14, pr|6,     "source",     (void *) source}, 
{15, pr|4,     "quit",       (void *) quit}, 
{16, pr|4,     "cold",       (void *) cold},
{16, pr|4,     "warm",       (void *) warm},
// Compiler	
{1,  pr|5,     ">body",      (void *) gbody}, 
{2,  pr|5,     "link>",      (void *) linkg}, 
{4,  pr|5,     "allot",      (void *) allot},
{5,  pr|im|5,  "ascii",      (void *) ascii}, 
{6,  pr|1,     ",",          (void *) comma}, 
{7,  pr|2,     "c,",         (void *) ccomma}, 
{8,  pr|2,     "w,",         (void *) wcomma},
{9,  pr|2,     "s,",         (void *) scomma}, 
{10, pr|1,     "'",          (void *) tick}, 
{11, pr|im|3,  "[']",        (void *) brackettick}, 
{12, pr|7,     "compile",    (void *) compile},
{13, pr|im|9,  "[compile]",  (void *) bracketcompile}, 
{14, pr|8,     "(create)",   (void *) docreate}, 
{15, pr|6,     "create",     (void *) createf}, 
{16, pr|7,     "<builds",    (void *) builds},
{17, pr|5,     "does>",      (void *) does}, 
{18, pr|9,     "immediate",  (void *) immediate}, 
{19, pr|im|8,  "postpone",   (void *) postpone}, 
{20, pr|1,     ":",          (void *) colon},
{21, pr|im|1,  ";",          (void *) semicolon}, 
{22, pr|im|2,  "s\"",        (void *) squote}, 
{23, pr|im|7,  "literal",    (void *) literal}, 
{24, pr|8,     "constant",   (void *) constant},
{25, pr|8,     "variable",   (void *) variable}, 
{27, pr|4,     "(to)",       (void *) dotof}, 
{28, pr|im|2,  "to",         (void *) tof},
{29, pr|5,     "defer",      (void *) defer}, 
{30, pr|6,     "defer@",     (void *) deferfetch}, 
{31, pr|6,     "defer!",     (void *) deferstore},
{32, pr|6,     "smudge",     (void *) smudge},
// Emit
{1,  pr|2,     "cr",         (void *) crf}, 
{2,  pr|5,     "space",      (void *) spacef}, 
{3,  pr|6,     "spaces",     (void *) spaces}, 
{4,  pr|4,     "type",       (void *) typef},
{5,  pr|1,     ".",          (void *) dot}, 
{5,  pr|2,     "h.",         (void *) hdot}, 
{5,  pr|2,     "d.",         (void *) ddot}, 
{6,  pr|2,     "u.",         (void *) udot}, 
{7,  pr|2,     ".r",         (void *) dotr}, 
{8,  pr|3,     "u.r",        (void *) udotr},
{9,  pr|im|2,  ".\"",        (void *) dotstring}, 
{10, pr|im|2,  ".(",         (void *) dotlparen}, 
{11, pr|im|1,  "(",          (void *) lparen}, 
{12, pr|4,     "hold",       (void *) hold},
{13, pr|2,     "<#",         (void *) sharpl}, 
{14, pr|1,     "#",          (void *) sharp}, 
{15, pr|2,     "#s",         (void *) sharps}, 
{16, pr|2,     "#>",         (void *) sharpg},
{17, pr|im|1,  "\\",         (void *) backslash}, 
{19, pr|2,     ".s",         (void *) dots}, 
{20, pr|3,     "ver",        (void *) ver},
// Vocabulary
{8,  pr|5,     "words",         (void *) wordsf},
// Device
{1,  pr|7,     "coretim",       (void *) coretim}, 
{2,  pr|9,     "emptydict",     (void *) Fempty}, 
{2,  pr|4,     "free",          (void *) Ffree}, 
{2,  pr|7,     "syssave",       (void *) syssave}, 
{2,  pr|10,    "sysrestore",    (void *) sysrestore}, 
{2,  pr|8,     "bootword",      (void *) bootword}, 
{2,  pr|5,     "reset",         (void *) reset}, 
{18, pr|10,    "emptyflash",    (void *) eraseflash}, 

#ifdef WITH_EEPROM
        {18, pr|2,     "e!",            (void *) FEEPROMStore}, 
        {18, pr|2,     "e@",            (void *) FEEPROMFetch}, 
        {18, pr|11,    "emptyeeprom",   (void *) FEraseEEPROM}, 
#endif

#ifdef WITH_FLASH_DEBUG
        {18, pr|8,     "NVMErase",      (void *) FNVMErase}, 
        {18, pr|8,     "NVMWrite",      (void *) FNVMWrite}, 
        {00, pr|9,     "findmagic",     (void *) Ffindmagic},
        {00, pr|13,    "findlastmagic", (void *) FfindLastMagic},
        {00, pr|13,    "findfreeflash", (void *) FFindFreeFlash},
        {00, pr|14,    "_findfreeflash",(void *) F_findFreeFlash},
        {00, pr|10,    "flashstart",    (void *) flashstart},
        {00, pr|8,     "flashend",      (void *) flashend},
#endif
#ifdef WITH_EXCEPTION_HANDLING
    #ifdef WITH_EEPROM
        {00, pr|16,    "getexceptioninfo",      (void *) getExceptionInfo},
    #endif
#endif


#include "Ud.h"

	{0xFF,pr,      "",           (void *) nop}

};
