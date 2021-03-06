

/*
* The program uses the 16X2 LCD, use the function dispinit() to initilize the LCD and display() to put the charachter on the lcd 
* The LCD works in the 4 bit mode with whole byte send pair of nibbles from MSB and LSB
* The pin arrangements are as follows 
* d4-->B8 of stm32
* d5-->B7 of stm32
* d6-->B6 of stm32
* d7-->A12 of stm32
* E-->A5 of stm32
* rs-->A6 of stm32
* displayint(index,integervalue,lineno) //to print the integer values to the LCD
* serial_printint(integervalue or variable); // to print the integer values to the serial port
*/

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include<string.h>
#include<stdlib.h>
#include<math.h>
#include"arm_usart.h"
#include"arm_lcd.h"
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/rtc.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/pwr.h>

#define version 1.0.1

#define one 200
#define two 220
#define three 252
#define four 204
#define five 236
#define six  196
#define seven 244
#define eight 164
#define nine 205
#define zero 133
#define off  148
#define ok  240
#define back 241
#define front 129
#define up 176
#define down 145
#define plus 224
#define minus 160
#define serialpd 1


/*************************GLOBAL STRUCTURES START***************************************/
typedef struct
{
	char ptokencode[12];
	uint8_t psec;
	uint8_t pmin;
	uint8_t phrs;
	uint8_t pdate;
	uint8_t pmonth;
	uint16_t pyear;
}past_token_values;
typedef struct
{
	uint8_t sec;
	uint8_t min;
	uint8_t hrs;
	uint8_t date;
	uint8_t month;
	uint16_t year;
} time;

time ctime = {
	.sec = 00,
	.min = 00,
	.hrs = 00,
	.date = 01,
	.month = 01,
	.year = 2017
};
time lcdon_time = {
	.sec=0,
	.min=0,
	.hrs=0
};

typedef struct
{
	int dollars;
	int minu;
	int validity;
} credit;

credit ocredit = 
{
	.dollars = 2
};

typedef struct
{
	char paygid[11];
	int pseudocode;
	char tokencode[12];
} creditadd;

creditadd cadd={
	.pseudocode=0
};

/**************************GLOBAL STRUCTURES END****************************************/

/*************************LIBRARY Function defenation start*****************************/
void button_setup(void);
void printir(void);
uint16_t powerfind(uint8_t no,uint8_t pow);
/*************************LIBRARY Function defenation end*****************************/



/*************************UTILITY Function defenation start*****************************/
void clearlcd(void);
void dealyn(uint8_t a);
void delay(void);
void unittest(void);
void displaytime(void);	 
void displaycreditmin(void);
void initial_time_display(void);
void displaycreditusd(void);
void displayvalidity(void);
void addcredittoken(void);
void displayprevioustokens(void);
void viewpaygid(void);
void displaytarrif(void);
//void inputtime(void);


void put_token_in_queue(char tok[12]);
void equate_token_values(char tok[12]);
//void ftoa(float f, char *str, uint8_t precision);
//uint8_t menu_function_divide(uint8_t a);


/*************************UTILITY Function defenation end*******************************/



/*************************global variables comes here *********************************/
//uint32_t a=0;
char tok1[5][12];
int tarrif=1; //tarrif as min/usd
uint8_t valid_add_option=0; //to enable the validity add to the previous validity afte token recharge make it  0 or 1 to replace the old validity with new one 
past_token_values ptoken[5];
uint8_t queue_index=0;
uint8_t state=1;
char code[16];
uint16_t hp=0,lp=0;
uint16_t sum=0;
uint16_t pulses[100][2];
uint8_t cp=0;
uint8_t testindex=0;
uint8_t df=1;
int delaycounter=0;
int delayvalue=0;
int testflag=0;
/**************************end global variables*******************************/


/**************************Button setup start*********************************/
void button_setup(void)  //setup for the IR PIN Read 
{
	rcc_periph_clock_enable(RCC_GPIOB); //enable clock for port b pin 9

  	gpio_set_mode(GPIOB, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO9); //enable gpio b9 pin as input 
  	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_10_MHZ,GPIO_CNF_OUTPUT_PUSHPULL, GPIO0);
	gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO15);
}
/**************************Button setup end***********************************/


/****************************************timer setup start******************************************/
static void tim_setup(void)
{
	rcc_periph_clock_enable(RCC_TIM2); /* Enable TIM2 clock. */
	nvic_enable_irq(NVIC_TIM2_IRQ);    /* Enable TIM2 interrupt. */
	rcc_periph_reset_pulse(RST_TIM2);   /* Reset TIM2 peripheral to defaults. */
	timer_set_mode(TIM2, TIM_CR1_CKD_CK_INT,TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
	timer_set_prescaler(TIM2, ((rcc_apb1_frequency) / 80000000)); /*sets the timer prescaler*/
     timer_disable_preload(TIM2);
	timer_continuous_mode(TIM2);
	timer_set_period(TIM2, 200); /* count full range, as we'll update compare value continuously */
     timer_enable_counter(TIM2);
	timer_enable_irq(TIM2, TIM_DIER_CC1IE);
}
/****************************************timer setup end******************************************/

/****************************************RTC interrupt Setup*************************************/
static void rtc_interrupt_setup(void)
{
	/* Without this the RTC interrupt routine will never be called. */
	nvic_enable_irq(NVIC_RTC_IRQ);
	nvic_set_priority(NVIC_RTC_IRQ, 1);
}

/****************************************RTC interrupt End***************************************/


/**************RTC interrupt subroutine start*********************/ 

void rtc_isr(void)
{
	static int secc=0;
	static int secv=0;
    	#ifdef serialpd
	//static int a=0; 	
	//serial_print("\r\n");
	//serial_printint(a);
	#endif
	rtc_clear_flag(RTC_SEC); // The interrupt flag isn't cleared by hardware, we have to do it.
     /*#ifdef serialpd
     //a=a+1;
     serial_printint(ctime.hrs);
     serial_print(":");
	serial_printint(ctime.min);
     serial_print(":");
	serial_printint(ctime.sec);
     serial_print("\r\n");
	#endif*/
     //clearlcd();
	if(gpio_get(GPIOA, GPIO15) && ocredit.validity>0)
	{
		secc++;
		secv++;
		if(secc>60 && ocredit.minu>0)
		{
			ocredit.minu--;
			secc=0;
			/*#ifdef serialpd
			serial_print("\r\n");
			serial_printint(secc);
			serial_print("\r\n");		
			#endif*/
			lcdon_time.min++;
			if(lcdon_time.min>60)
			{
				lcdon_time.min=0;
				lcdon_time.hrs++;
			}
		}
		if(ocredit.validity==0)
		{
			ocredit.minu=0;
		}
		if(secv>86400 && ocredit.validity>0)
		{
			ocredit.validity--;
			if(ocredit.validity==0)
			{
				ocredit.minu=0;
			}
		}
	}
			
	ctime.sec++;
     if(ctime.sec==60)
     {
     	ctime.sec=00;
     	ctime.min++;
     	if(ctime.min==60)
     	{
     		ctime.min=00;
     		ctime.hrs++;
     		if(ctime.hrs==24)
     		{
     			ctime.hrs=00;
     			ctime.date++;
				if(ctime.date==29 && ctime.month==2 && ctime.year%4==0 && ctime.year%100!=0)
				{
     				ctime.date=01;
     				ctime.month++;
                         if(ctime.month==12)
					{
  						ctime.month=01;
						ctime.year++;

					}     			
				}
				if(ctime.date==29 && ctime.month==2 && ctime.year%4==0 && ctime.year%100==0 && ctime.year%400==0)
				{
     				ctime.date=01;
     				ctime.month++;
                         if(ctime.month==12)
					{
  						ctime.month=01;
						ctime.year++;

					}     			
				}

     			if(ctime.date==28 && ctime.month==2)
     			{
     				ctime.date=01;
     				ctime.month++;
                         if(ctime.month==12)
					{
  						ctime.month=01;
						ctime.year++;

					}     			
				}
     			if(ctime.date==30 && ((ctime.month==4) | (ctime.month==6) | (ctime.month==9) | (ctime.month==11)))
     			{
					ctime.date=01;
     				ctime.month++;
					if(ctime.month==12)
					{
						ctime.month=01;
						ctime.year++;

					}
				}
                    
 				if(ctime.date==31 && ((ctime.month==1) | (ctime.month==3) | (ctime.month==5) | (ctime.month==7) | (ctime.month==8)))
     			{
					ctime.date=01;
     				ctime.month++;
					if(ctime.month==12)
					{
						ctime.month=01;
						ctime.year++;

					}				

				}      			
     			
     		}
		}
	}
     		
}
/**************RTC interrupt subroutine end***********************/ 

/*************************TIMER interrupt routine*****************/
void tim2_isr(void)
{ 
	if(testflag==0)
	{
		if(gpio_get(GPIOA, GPIO15) && ocredit.minu>0 && ocredit.validity>0)
		{
     		gpio_high(GPIOA, GPIO0);
		}
     	else
		{
			gpio_low(GPIOA, GPIO0);
		}
	}
	if (timer_get_flag(TIM2, TIM_SR_CC1IF))  // clock check code begin  
	{
		timer_clear_flag(TIM2, TIM_SR_CC1IF);  //check if the overflowtimer flag is clear 
  		
  		/*********** ir reading code begin ************/
		if(state==1)
  		{
   			//if(PIND & (1<<2))
  			if(!(gpio_get(GPIOB, GPIO9)))
   			{
    				hp++;
    				//if(hp>650 && cp!=0)
    				if(hp>450 && cp!=0)
     			{
      				printir();
      				cp=0;
    	 			}
   			}	
   			else	
   			{
    				pulses[cp][0]=hp;	
    				state=0;
    				hp=0;
    				lp=0;
    				sum=0;
   			}
  		}	

  		if(state==0)
  		{
   			//if(! (PIND & _BV(2)))
   			if(gpio_get(GPIOB, GPIO9))
   			{
    				lp++;
    				//if(lp>650 && cp!=0)
      			if(lp>450 && cp!=0)
     			{
      				printir();
      				cp=0;
     			}
   			}	
   			else	
   			{
    				pulses[cp][1]=lp;	
    				state=1;
    				hp=0;
    				lp=0;
    				cp++;	 
   			}
  		}
 		/*********** ir reading code begin ************/

	}

}
/*************************TIMER interrupt routine end*****************/



/*************************IR Value Print routine start*****************/
void printir(void)
{
  	serial_print("\n");
 	uint8_t z;
 	uint8_t p=7;
	serial_print("\r\nthe code starts\r\n");
 	for (z = 17; z < 25; z++) 
  	{ 
  		uint16_t xa=(pulses[z][1]*50);
    		#ifdef serialpd
    		itoa(xa,code,10);
    		serial_print("\r\n the low pulse is :");
    		serial_print(code);
    		serial_print("\t");
    		#endif
    		uint16_t ya=(pulses[z+1][0]*50);
    		#ifdef serialpd
    		itoa(ya,code,10);
    		serial_print("\r\n the high pulse is :");
    		serial_print(code);
    		#endif
    		if(xa<=3000 && ya<=3000)
    		{
    			sum=sum+0;
    			#ifdef serialpd
    			serial_print("\r\n 0");
    			#endif
    		}
    		else
    		{
    			#ifdef serialpd
    			serial_print("\r\n 1");
    			#endif
    			sum=sum+(powerfind(2,p));
    		}  
   		p--;
  	}
  	#ifdef serialpd
  	//display(1,"the IR code is",1);
  	itoa(sum,code,10);
  	serial_print("\r\n the ir code is ");
  	serial_print(code);
  	//char cf[16];
  	//stringformat(code,cf);
  	//display(1,cf,2);
  	#endif
	return;
}
/*************************IR Value Print routine end*****************************/


/*************************SUBROUTINE to find the power***************************/
uint16_t powerfind(uint8_t no,uint8_t pow)
{
	uint16_t mu=1;
	uint8_t i;
	for(i=0;i<pow;i++)
	{
		mu=mu*no;
	}
	return mu;
}
/*************************SUBROUTINE to find the power END**************************/


int main(void)
{
	dispinit();  //LCD is initialized
   	button_setup();  //initialize the input GPIO
	tim_setup(); //setup the timer
	usart3_init(); //initialize the GPIO
  	//display(1,"it's working ",1); //to write on the LCD 1st argument takes the starting position of LCD, 3rd argument is the Line no    
     display(1,"TURN on the PAYG",1); 	 //initial display on screen
   	rtc_auto_awake(RCC_LSE, 0x7fff); // initializing the RTC
	rtc_interrupt_setup();
	rtc_interrupt_enable(RTC_SEC);
	ocredit.minu=ocredit.dollars; //monetary to time credit conversion
	strcpy(cadd.paygid,"11234567890");
     //int af=0;
     

   	clearlcd();
	while (1) 
   	{
		start:
   	/**************************initial display*************************************/
   	/////////////////////////////////////////////////////////////////////////////// 
		if(ocredit.minu>0 && ocredit.validity>0)
		{   		
			display(1,"100%",1);
   			if(gpio_get(GPIOA, GPIO15))
   			{
   				display(6,"ON ",1);
   		
   			}
   			else
   			{
   				display(6,"OFF",1);
   			}
   			if(lcdon_time.hrs<10)
   			{
   				displayint(12,lcdon_time.hrs,1);
   				display(13,"H",1);
   				if(lcdon_time.min<10)
   				{
   					display(14,"0",1);
   					displayint(15,lcdon_time.min,1);
   					display(16,"M",1);
   				}
   				if(lcdon_time.min>9)
   				{
   					displayint(14,lcdon_time.min,1);
   					display(16,"M",1);
   				}
   			}	
   			if(lcdon_time.hrs>9 && lcdon_time.hrs<100)
   			{
   				displayint(11,lcdon_time.hrs,1);
   				display(13,"H",1);
   				if(lcdon_time.min<10)
   				{
   					display(14,"0",1);
   					displayint(15,lcdon_time.min,1);
   					display(16,"M",1);
   				}
   				if(lcdon_time.min>9)
   				{
   					displayint(14,lcdon_time.min,1);
   					display(16,"M",1);
   				}
   			}
   			if(lcdon_time.hrs>99)
   			{
   				displayint(10,lcdon_time.hrs,1);
   				display(13,"H",1);
   				if(lcdon_time.min<10)
   				{
   					display(14,"0",1);
   					displayint(15,lcdon_time.min,1);
   					display(16,"M",1);
   				}
   				if(lcdon_time.min>9)
   				{
   					displayint(14,lcdon_time.min,1);
   					display(16,"M",1);
   				}
   			}
		}
		else
		{
			if(ocredit.validity==0 && ocredit.minu==0 )
			{	
				display(1,"NO credit & val  ",1);
			}
			if(ocredit.validity==0 && ocredit.minu>0)
			{
				display(1,"NO validity      ",1);
			}
			if(ocredit.validity>0 && ocredit.minu==0)
   			{
				display(1,"NO Credit        ",1);
			}
		}	
		initial_time_display();
/********************************initial display*************************************/
///////////////////////////////////////////////////////////////////////////////////// 
   		if(sum==ok)
   		{
   			sum=0;
   			while(1)
   			{
   				loop1:
				display(1,"1. test unit  ",1);
				display(1,"2. Display time ",2);
				if(sum==one)
				{
					sum=0;
					unittest();
				}
				if(sum==two)
				{
					sum=0;
					displaytime();
				}
				if(sum==up)
				{
					sum=0;
					clearlcd();
					goto loop9;
					//menu_function_divide(1);
				}
				if(sum==back)
				{
					sum=0;
					clearlcd();
					goto start;
				}
				if(sum==down)
				{
					sum=0;
					clearlcd();
					goto loop2;
                    }					
			}			
			while(1)
			{
				loop2:
				display(1,"2. Display time ",1);
	 			display(1,"3. disp Cr. MIN ",2);
				if(sum==two)
				{
					sum=0;
					displaytime();
				}
				if(sum==three)
				{
					sum=0;
			    		displaycreditmin();	 
				}
				if(sum==up)
				{ 
					sum=0;
					clearlcd();
					goto loop1;
				}
				if(sum==back)
				{
					sum=0;
					clearlcd();
					goto start;
				}
				if(sum==down)
				{   
					sum=0;
					clearlcd();
					goto loop3;
				}
			}
			while(1)
			{
				loop3:
				display(1,"3. Disp Cr. MIN ",1);
	         		display(1,"4. Disp Cr. USD.",2);
				if(sum==three)
				{
					sum=0;
					displaycreditmin();
				}
				if(sum==four)
				{
					sum=0;
					displaycreditusd();
				}
				if(sum==up)
				{
					sum=0;
					clearlcd();
					goto loop2;
				}
				if(sum==back)
				{
					sum=0;
					clearlcd();
					goto start;
				}
				if(sum==down)
				{
					sum=0;
					clearlcd();
					goto loop4;
				}
			}
			while(1)
			{
				loop4:
				display(1,"4. Disp Cr. USD.",1);
				display(1,"5. ADD TOKEN   ",2);
				if(sum==four)
				{
					sum=0;
					displaycreditusd();
				}
				if(sum==five)
				{
					sum=0;
					addcredittoken();
				}
				if(sum==up)
				{
					sum=0;
					clearlcd();
					goto loop3;
				}
				if(sum==back)
				{
					sum=0;
					clearlcd();
					goto start;
				}
				if(sum==down)
				{
					sum=0;
					clearlcd();
					goto loop5;
				}
			}			
			while(1)
			{
				loop5:
				display(1,"5. ADD TOKEN    ",1);
				display(1,"6. edit time    ",2);
				if(sum==five)
				{
					sum=0;
					addcredittoken();
				}
				if(sum==six)
				{
					sum=0;
					//inputtime();
				}
				if(sum==up)
				{
					sum=0;
					clearlcd();
					goto loop4;
				}
				if(sum==back)
				{
					sum=0;
					clearlcd();
					goto start;
				}
				if(sum==down)
				{
					sum=0;
					clearlcd();
					goto loop6;
				}	
			}
			while(1)
			{   
				loop6:
				display(1,"6. edit time    ",1);
				display(1,"7. show validity",2);
				if(sum==six)
				{
					sum=0;
					//inputtime();
				}
				if(sum==seven)
				{
					sum=0;
					displayvalidity();
				}
				if(sum==up)
				{
					sum=0;
					clearlcd();
					goto loop5;
				}
				if(sum==back)
				{
					sum=0;
					clearlcd();
					goto start;
				}
				if(sum==down)
				{
					sum=0;
					clearlcd();					
					goto loop7;				
				}
			}
			while(1)
			{
				loop7:
				display(1,"7. show validity",1);
				display(1,"8.Disp Last 20 T",2);
				if(sum==seven)
				{
					sum=0;
					displayvalidity();
				}
				if(sum==eight)
				{
					sum=0;
					displayprevioustokens();
				}
				if(sum==up)
				{
					sum=0;
					clearlcd();
					goto loop6;
				}
				if(sum==back)
				{
					sum=0;
					clearlcd();
					goto start;
				}
				if(sum==down)
				{	
					sum=0;
					clearlcd();
					goto loop8;				
				}
			}
			while(1)
			{
				loop8:
				display(1,"8.Disp Last 20 T",1);
				display(1,"9.View PAYG ID  ",2);
				if(sum==nine)
				{
					sum=0;
					viewpaygid();
				}
				if(sum==eight)
				{
					sum=0;
					displayprevioustokens();
				}
				if(sum==up)
				{
					sum=0;
					clearlcd();
					goto loop7;
				}
				if(sum==back)
				{
					sum=0;
					clearlcd();
					goto start;
				}
				if(sum==down)
				{	
					sum=0;
					clearlcd();
					goto loop9;				
				}
			}
			while(1)
			{
				loop9:
				display(1,"9.View PAYG ID  ",1);
				display(1,"10.View Tarrif  ",2);
				if(sum==nine)
				{
					sum=0;
					viewpaygid();
				}
				if(sum==zero)
				{
					sum=0;
					displaytarrif();
				}
				if(sum==up)
				{
					sum=0;
					clearlcd();
					goto loop8;
				}
				if(sum==back)
				{
					sum=0;
					clearlcd();
					goto start;
				}
				if(sum==down)
				{	
					sum=0;
					clearlcd();
					goto loop1;				
				}
			}
		}
	}
	return 0;
}


void delay(void)
{
	for(int i=0;i<5000000; i++)
	{
	 __asm__("nop");
	}
	return;
}

void dealyn(uint8_t a)
{
	for(int i=0;i<(500000*a); i++)
	{
	 __asm__("nop");
	}
	return;
}

void clearlcd(void)
{
	display(1,"                ",1);
	display(1,"                ",2);
 	return;
}


void unittest(void)
{
	testflag=1;
	clearlcd();
	if(testindex==4)
	{
		display(1,"max test limit  ",1);
		display(1,"reached         ",2);
          delay();
		testflag=0;
		return;
	}
	
	else
	{
	testindex++;
	display(1,"light'll stay on",1);
	display(1,"for few seconds ",2);
	//or=0;
     gpio_high(GPIOA, GPIO0);
   	delay();
	gpio_low(GPIOA, GPIO0);
	//or=1;
	testflag=0;
	return ;
	}
}

void displaytime(void)
{
	clearlcd();
	display(1,"Current time is ",1);
	dealyn(5);
	while(1)
	{
		if(ctime.hrs<10)
		{
			displayint(2,ctime.hrs,1);
 			display(1,"0",1);
			display(9," ",1);}
		else
		{
			displayint(1,ctime.hrs,1);
			display(9," ",1);
		}
		display(3,":",1);
		
		if(ctime.min<10)
		{
			displayint(5,ctime.min,1);
			display(4,"0",1);
			display(9," ",1);
		}
		else
		{
			displayint(4,ctime.min,1);
		}
		display(6,":",1);
		
		if(ctime.sec<10)
		{
			displayint(8,ctime.sec,1);
 			display(7,"0",1);
	 		display(9," ",1);
		}
		else
		{
			displayint(7,ctime.sec,1);
		}
		display(9,"        ",1);
		if(ctime.date<10)
		{
			display(1,"0",2);
			displayint(2,ctime.date,2);
		}
		else
		{
			displayint(1,ctime.date,2);
		}
		display(3,"/",2);
		if(ctime.month==1)
		{
			display(4,"JAN",2);
		}
		if(ctime.month==2)
		{
			display(4,"FEB",2);
		}
		if(ctime.month==3)
		{
			display(4,"MAR",2);
		}
		if(ctime.month==4)
		{
			display(4,"APR",2);
		}
		if(ctime.month==5)
		{
			display(4,"MAY",2);
		}
		if(ctime.month==6)
		{
			display(4,"JUN",2);
		}
		if(ctime.month==7)
		{
			display(4,"JUL",2);
		}
		if(ctime.month==8)
		{
			display(4,"AUG",2);
		}
		if(ctime.month==9)
		{
			display(4,"SEP",2);
		}
		if(ctime.month==10)
		{
			display(4,"OCT",2);
		}
		if(ctime.month==11)
		{
			display(4,"NOV",2);
		}
		if(ctime.month==12)
		{
			display(4,"DEC",2);
		}
		display(7,"/",2);
		displayint(8,ctime.year,2);
		display(12,"     ",2);
		if(sum==ok)
		{
			return;
		}
	}
}
void initial_time_display(void)
{
	if(ctime.hrs<10)
	{
		displayint(2,ctime.hrs,2);
 		display(1,"0",2);
 		display(9," ",2);}
	else
	{
		displayint(1,ctime.hrs,2);
		display(9," ",2);
	}
	display(3,":",2);
		
	if(ctime.min<10)
	{
		displayint(5,ctime.min,2);
		display(4,"0",2);
		display(9," ",2);
	}
	else
	{
		displayint(4,ctime.min,2);
	}
	display(6,":",2);
		
	if(ctime.sec<10)
	{
		displayint(8,ctime.sec,2);
 		display(7,"0",2);
		display(9," ",2);
	}
	else
	{
		displayint(7,ctime.sec,2);
	}
	display(9," ",2);
	if(ctime.date<10)
	{
		display(11,"0",2);
		displayint(12,ctime.date,2);
	}
	else
	{
		displayint(11,ctime.date,2);
	}
	display(13,"/",2);
	if(ctime.month==1)
	{
		display(14,"JAN",2);
	}
	if(ctime.month==2)
	{
		display(14,"FEB",2);
	}
	if(ctime.month==3)
	{
		display(14,"MAR",2);
	}
	if(ctime.month==4)
	{
		display(14,"APR",2);
	}
	if(ctime.month==5)
	{
		display(14,"MAY",2);
	}
	if(ctime.month==6)
	{
		display(14,"JUN",2);
	}
	if(ctime.month==7)
	{
		display(14,"JUL",2);
	}
	if(ctime.month==8)
	{
		display(14,"AUG",2);
	}
	if(ctime.month==9)
	{
		display(14,"SEP",2);
	}
	if(ctime.month==10)
	{
		display(14,"OCT",2);
	}
	if(ctime.month==11)
	{
		display(14,"NOV",2);
	}
	if(ctime.month==12)
	{
		display(14,"DEC",2);
	}
	
}

void displaycreditmin(void)
{
	clearlcd();
	while(1)
	{
		display(1," Credit min are ",1);
		displayint(2,ocredit.minu,2);
        	display(7,"MIN",2);
        	if(sum==ok)
	 	{
			sum=0;
			return;
	 	}
	}
}

void displaycreditusd(void)
{
	clearlcd();
	while(1)
	{
		display(1,"Credit in USD is",1);
		//ftoa(ocredit.minu/tarrif,str,2);
		displayint(2,ocredit.minu/tarrif,2);
		display(12,"USD",2);
		if(sum==ok)
	 	{
			sum=0;
			return;
	 	}
	}
}
void displayvalidity(void)
{
	clearlcd();
	while(1)
	{
		display(1,"Curent Validity ",1);
		displayint(2,ocredit.validity,2);
        	display(7,"Days",2);
        	if(sum==ok)
	 	{
			sum=0;
			return;
	 	}
	}
}
void put_token_in_queue(char tok[12])
{
	#ifdef serialpd	
	serial_print("\r\nthe token value in funcccccccccccccccccctionnnnnnnnnnnnnnnnn issssss: \r\n");
	serial_print(tok);
	serial_print("\r\nthe queue index is \r\n");
	serial_printint(queue_index);
	#endif
	if(queue_index==4)
	{
		for(uint8_t a=0;a<5;a++)
		{
			//ptoken[a]=ptoken[a+1];
			strcpy(tok1[a],tok[a+1]);		
		}
		equate_token_values(tok);
	}
	else
	{
		equate_token_values(tok);
		queue_index++;
	}		
	return;
}
void equate_token_values(char tok[12])
{
	
	#ifdef serialpd	
	serial_print("\r\ninnnnnnn theeeeeeee equattttttteeeeeee token:: \r\n");
	serial_print(tok);
	serial_print("\r\nthe queue index is \r\n");
	serial_printint(queue_index);
	#endif
	//strcpy(ptoken[queue_index].ptokencode,tok);
	strcpy(tok1[queue_index],tok);	
	#ifdef serialpd	
	serial_print("\r\nthe equatadddddddddddddddd valuessssssssssss areeeeeeeee:: \r\n");
	//serial_print(ptoken[queue_index].ptokencode);
	serial_print(tok1[queue_index]);
	/*ptoken[queue_index].psec=ctime.sec;
	ptoken[queue_index].pmin=ctime.min;
	ptoken[queue_index].phrs=ctime.hrs;
	ptoken[queue_index].pdate=ctime.date;
	ptoken[queue_index].pmonth=ctime.month;
	ptoken[queue_index].pyear=ctime.year;*/
	
	serial_print("\r\nthe queue index is \r\n");
	serial_printint(queue_index);
	#endif
	return;
}
void displayprevioustokens(void)
{
	clearlcd();
	uint8_t a=0; 
	uint8_t b=1;
	while(1)
	{
		displayint(1,b,1);
		display(2,".",1);
		display(3,tok1[a],1);
		if(queue_index>0)
		{
			if(sum==down && b==0)
			{
				sum=0;
				clearlcd();
				b=queue_index;
				a=1;
			}
			if(sum==down && b!=0)
			{
				sum=0;
				clearlcd();
				b--;
				a++;
			}
			if(sum==up && b!=queue_index)
			{
				sum=0;
				clearlcd();
				b++;
				a--;
			}
			if(sum==up && b==queue_index)
			{
				sum=0;
				clearlcd();
				b=0;
				a=queue_index;
			}
			
		}
		if(sum==back)
		{
			sum=0;
			clearlcd();
			return;
		}
			
	}
}

/*void displayprevioustokens(void)
{
	clearlcd();
	uint8_t a=queue_index; 
	uint8_t b=1;
	while(1)
	{
		displayint(1,b,1);
		display(2,".",1);
		display(3,ptoken[a].ptokencode,1);
		if(ptoken[a].phrs<10)
		{
			displayint(2,ptoken[a].phrs,2);
			display(1,"0",2);
		}
		else
		{
			displayint(1,ptoken[a].phrs,2);
		}
		display(3,":",2);
		if(ptoken[a].pmin<10)
		{
			displayint(5,ptoken[a].pmin,2);
			display(4,"0",2);
		}
		else
		{
			displayint(4,ptoken[a].phrs,2);
		}
		if(ptoken[a].pdate<10)
		{
			display(7,"0",2);
			displayint(8,ptoken[a].pdate,2);
		}
		else
		{
			displayint(7,ptoken[a].pdate,2);
		}
		display(9,"/",2);
		if(ptoken[a].pmonth==1)
		{
			display(10,"JAN",2);
		}
		if(ptoken[a].pmonth==2)
		{
			display(10,"FEB",2);
		}
		if(ptoken[a].pmonth==3)
		{
			display(10,"MAR",2);
		}
		if(ptoken[a].pmonth==4)
		{
			display(10,"APR",2);
		}
		if(ptoken[a].pmonth==5)
		{
			display(10,"MAY",2);
		}
		if(ptoken[a].pmonth==6)
		{
			display(10,"JUN",2);
		}
		if(ptoken[a].pmonth==7)
		{
			display(10,"JUL",2);
		}
		if(ptoken[a].pmonth==8)
		{
			display(10,"AUG",2);
		}
		if(ptoken[a].pmonth==9)
		{
			display(10,"SEP",2);
		}
		if(ptoken[a].pmonth==10)
		{
			display(10,"OCT",2);
		}
		if(ptoken[a].pmonth==11)
		{
			display(10,"NOV",2);
		}
		if(ptoken[a].pmonth==12)
		{
			display(10,"DEC",2);
		}
		display(13,"/",2);
		displayint(14,ptoken[a].pyear%1000,2);
		display(12,"     ",2);
		if(queue_index>0)
		{
			if(sum==down && b==0)
			{
				sum=0;
				b=queue_index;
				a=1;
			}
			if(sum==down && b!=0)
			{
				sum=0;
				b--;
				a++;
			}
			if(sum==up && b!=queue_index)
			{
				sum=0;
				b++;
				a--;
			}
			if(sum==up && b==queue_index)
			{
				sum=0;
				b=0;
				a=queue_index;
			}
			
		}
		if(sum==back)
		{
			sum=0;
			return;
		}	
	}
}*/

void viewpaygid(void)
{
	clearlcd();	
	while(1)
	{	
		display(1,"PAYG ID is   :  ",1);
		display(1,cadd.paygid,2);
		if(sum==ok)
		{
			sum=0;
			return;
		}
	}
}

void displaytarrif(void)
{
	clearlcd();
	while(1)
	{
		display(1,"The Tarrif is : ",1);
		displayint(2,tarrif,2);
		display(6,"MIN/USD    ",2);
		if(sum==ok)
		{
			sum=0;
			return;
		}
	}
	
}

void addcredittoken(void)
{ 
	uint8_t stc=0;
	char tmptoken[12];
	char tmparray[9];
     char tmparray2[6];
	char tokenvalid[2];
	char tokencredit[4];
	char tokenlastsix[6];
	char paygidpseudo[6];
	clearlcd();
  	//passkey();
 	display(1,"Enter the token ",1);
	while(1)
	{
  
  		/*if(test==0)
  		{
		return;
  		}*/
		#ifdef serialpd
  		serial_print("\r\n the code is inside while loop \r\n");
		#endif 		
		while(stc<12)
 		{
			#ifdef serialpd
  			//serial_print("\r\n the code is inside another while loop \r\n");
			#endif
			if(sum==back)
			{
				backas:
				sum=0;
				if(stc>0)
				{
					cadd.tokencode[stc]=' ';
					display((stc+1)," ",2);					
					stc--;
				}
				if(stc==0)
				{
					return;
				}			
			}
  			if(sum==one)
  			{
    				sum=0;
				cadd.tokencode[stc]='1';
    				stc++;
				display((stc+1),"1",2);
  			}
  			if(sum==two)
  			{
  				sum=0;
  				cadd.tokencode[stc]='2';
  				stc++;
				display((stc+1),"2",2);
  			}
  			if(sum==three)
  			{
  				sum=0;
  				cadd.tokencode[stc]='3';
  				stc++;
				display((stc+1),"3",2);
  			}
  			if(sum==four)
  			{
  				sum=0;
  				cadd.tokencode[stc]='4';
  				stc++;
				display((stc+1),"4",2);
  			} 
  			if(sum==five)
  			{ 
  				sum=0;
  				cadd.tokencode[stc]='5';
  				stc++;
				display((stc+1),"5",2);
  			} 
  			if(sum==six)
  			{
  				sum=0;
  				cadd.tokencode[stc]='6';
  				stc++;
				display((stc+1),"6",2);
  			}
  			if(sum==seven)
  			{
  				sum=0;
  				cadd.tokencode[stc]='7';
  				stc++;
				display((stc+1),"7",2);
  			}
  			if(sum==eight)
  			{
  				sum=0;
  				cadd.tokencode[stc]='8';
  				stc++;
				display((stc+1),"8",2);
  			}
  			if(sum==nine)
  			{
  				sum=0;
  				cadd.tokencode[stc]='9';
  				stc++;
				display((stc+1),"9",2);
  			}
  			if(sum==zero)
  			{
  				sum=0;
  				cadd.tokencode[stc]='0';
  				stc++;
				display((stc+1),"0",2);
  			} 
  			else
  			{
  				continue;
  			}
			//serial_print("\r\n");
			//serial_print(cadd.tokencode);
			//serial_print("\r\n");  			
			//display(1,cadd.tokencode,2);

 		}
		serial_print("\r\n");
		serial_print(cadd.tokencode);
		serial_print("\r\n");
		if(sum==back)
		{
			goto backas;
		}
   		if(sum==ok)
   		{
           	sum=0;
	       	uint8_t cunt;
           	uint8_t two_t=0;
           	uint8_t four_t=0;
           	uint8_t six_t=0;
      		for(cunt=0;cunt<12;cunt++)
        		{
           		if(cunt<2)
             		{
               		tokenvalid[two_t]=cadd.tokencode[cunt];
               		two_t++;
             		}   
				if(cunt>=2 && cunt<6)
             		{
               		tokencredit[four_t]=cadd.tokencode[cunt];
               		four_t++;
             		} 
	    			if(cunt>=6 && cunt<12)
             		{
               		tokenlastsix[six_t]=cadd.tokencode[cunt];
               		six_t++;
             		}
        		}
			int tmpvalid;
			int tmpcredit;
			int tmplastsix;
			int paygidpseudotmp;
			tmpvalid=atoi(tokenvalid);
			tmpcredit=atoi(tokencredit);
			tmplastsix=atoi(tokenlastsix);
			#ifdef serialpd			
			serial_print("\r\n");
			serial_print("the credit is: ");
			serial_printint(tmpcredit);
			serial_print("\r\n");
			serial_print("\r\n");
			serial_print("the validity is:");			
			serial_printint(tmpvalid);
			serial_print("\r\n");
			serial_print("\r\n");
			serial_print("the last six value is:");			
			serial_printint(tmplastsix);
			serial_print("\r\n");	 
			#endif  		
			uint8_t idc;
	   		uint8_t idcp=0;
	   		for(idc=5;idc<11;idc++)
	   		{
		   		paygidpseudo[idcp]=cadd.paygid[idc];
           		idcp++;	   
	   		}
			#ifdef serialpd
			serial_print("\r\n");
			serial_print("the last six of paygid is:");			
			serial_print(paygidpseudo);
			serial_print("\r\n");
			#endif
	   		paygidpseudotmp=atoi(paygidpseudo);
	   		int tmp;
			tmp=paygidpseudotmp+cadd.pseudocode;
			if(tmp>999999)
			{
				tmp=tmp/10;
				tmp=(tmp-tmpcredit)+tmpvalid;
			}
			else
			{
				tmp=(tmp-tmpcredit)+tmpvalid;
			}
			tmp=(tmp*(tmp/1000));
			itoa(tmp,tmparray,10);
			idcp=0;
			#ifdef serialpd
			serial_print("\r\n");
			serial_print("the temporary square value is:");			
			serial_printint(tmp);
			serial_print("\r\n");
			#endif
			
			#ifdef serialpd
			serial_print("\r\n");
			serial_print(" the temp array string is ");			
			serial_print(tmparray);
			serial_print("\r\n");
			#endif
			idc=strlen(tmparray);
			for(uint8_t idci=idc-6;idci<idc;idci++)
			{
				tmparray2[idcp]=tmparray[idci];
				idcp++;
			}
			#ifdef serialpd
			serial_print("\r\n");
			serial_print("the temporary 2nd value is :");			
			serial_print(tmparray2);
			serial_print("\r\n");
			#endif
			tmp=atoi(tmparray2);
			#ifdef serialpd
			serial_print("\r\n");
			serial_print("the temporary value from the code is:");			
			serial_printint(tmp);
			serial_print("\r\n");
			#endif
			if(tmp==tmplastsix)
	   		{
				clearlcd();
				ocredit.dollars=ocredit.dollars+tmpcredit;
				ocredit.minu=ocredit.dollars*tarrif;
				testindex=0;
				if(valid_add_option==0)
				{
					ocredit.validity=ocredit.validity+tmpvalid;
				}
				else
				{
					ocredit.validity=tmpvalid;
				}
				display(1,"correct token",1);
				serial_print("the token isssssssssssss corecccccccccccccctttttttttttttttttttttttt \r\n");				
				for(int i=0;i<2000000; i++)
				{
	 				__asm__("nop");
				}
				cadd.pseudocode=tmplastsix;
				lcdon_time.sec=0;
				lcdon_time.min=0;
				lcdon_time.hrs=0;
				#ifdef serialpd
				serial_print("\r\n");
				serial_print("the pseudo code is:");			
				serial_printint(cadd.pseudocode);
				serial_print("\r\n");
				serial_print("\r\n");
				serial_print("the token codeeeeeeeeeeeeeeeeeeeeeeeeeeee  is:::::::::: \r\n");			
				serial_print(cadd.tokencode);				
				#endif
				put_token_in_queue(cadd.tokencode);  
				return;	   		
			}
	   		else
	   		{
				clearlcd();
       			display(1,"incorrect token",1);
	   			for(int i=0;i<2000000; i++)
				{
	 				__asm__("nop");
				}
	   			return;
	  		}
	   	}
 	}
}











