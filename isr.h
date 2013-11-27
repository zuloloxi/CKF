/*******************************************************************************
 * ChipKitForth
 * Interactive Forth environment for PIC32 based ChipKit boards.
 * Based on DIOSFORTH. http://www.forth.cz/Download/DIOSForth/DIOSForth.html
 * Developed under MPIDE.
 * Public repository: https://github.com/jvvood/ChipKitForth
 * Published under GPLv3.
 * Created by Janos Waldhauser (2013).
 * Email: janos.waldhauser@gmail.com
 ******************************************************************************/
 

/*******************************************************************************
 * 
 * 
 * 
 * 
 * 
 * 
 ******************************************************************************/

#include "Config.h"

#ifdef WITH_ISR

#ifndef __ISR_H__
#define __ISR_H__

#include <plib.h>
#include <p32xxxx.h>
#include "Config.h"
#include "GenericTypeDefs.h"
#include "GenericTypeDefs.h"
#include "WProgram.h"


#define ISR_DATA_SIZE  4

/* 
 * Max 32 ISR source
 * Every ISR source has a bit in isr_source
 * Currenty only 2 has implemented
 */
enum ISR_SOURCES {
  ISR_SOURCE_CORE_TIMER = 0,
  ISR_SOURCE_PIN_CHANGE = 1,
  ISR_SOURCE_LAST
};


/*
 * Array for passing datas from C to FORTH
 */
extern UINT isr_data[ISR_SOURCE_LAST][ISR_DATA_SIZE];

/*
 * Array of ISR processing FORTH words
 */
extern UINT isr_words[ISR_SOURCE_LAST];
/*
 * C level ISR function sets the assigned bit to 1
 */
extern UINT isr_source;

/*
 * ISR processed only if the corresponding bit setted to 1
 */
extern UINT isr_mask;

/*
 * Inicates that ISR is currently processing on the FORTH level
 */
extern UINT isr_processing;


extern UINT isr_enabled;


extern void isrw(void);
extern void initIsr(void);
extern void isrdisable(void);

extern void isrdata(void);
extern void isrenable(void);
extern void isrdisable(void);
extern void isrwords(void);
extern void isrmask(void);
extern void isrsource(void);
extern void c_isrdatasize(void);
extern void setisr(void);
extern void enableisr(void);
extern void disableisr(void);
extern void isrdatafetch(void);
extern void pinchanged(void);

#ifdef WITH_CORETIM_ISR
extern void initCoreTimerIsr(void);
extern uint32_t myCoreTimerService(uint32_t curTime);
extern void uptime(void);
extern UINT uptime_sec;

#ifdef WITH_LOAD_INDICATOR
extern UINT load_counter, load_value;
extern void load(void);
#endif  // WITH_LOAD_INDICATOR
extern void c_coretimer(void);
#endif  // WITH_CORETIM_ISR

#ifdef WITH_PINCHANGE_ISR
extern void pinToCN(void);
extern void pinFromCN(void);
extern void c_pinchange(void);
#endif

#endif // __ISR_H__
#endif  // WITH_ISR

