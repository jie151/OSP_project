#include "stdio.h"
#include "includes.h"

/*
*********************************************************************************************************
*                                              CONSTANTS
*********************************************************************************************************
*/
#define          TASK_STK_SIZE     512                /* Size of each task's stacks (# of WORDs)       */

#define          TASK_START_ID       0                /* Application tasks IDs                         */
#define          TASK_CLK_ID         1

#define          TASK_START_PRIO    10                /* Application tasks priorities                  */
#define          TASK_CLK_PRIO      11

#define		PERIODIC_TASK_START_ID  20
#define		PERIODIC_TASK_START_PRIO  20

typedef struct{
	INT32U RemainTime;
	INT32U ExecutionTime;
	INT32U Period;
	INT32U Deadline;
} TASK_EXTRA_DATA;

/*
*********************************************************************************************************
*                                              VARIABLES
*********************************************************************************************************
*/

OS_STK        TaskStartStk[TASK_STK_SIZE];            /* Startup    task stack                         */
OS_STK        TaskClkStk[TASK_STK_SIZE];              /* Clock      task stack                         */

OS_STK TaskStk[8][TASK_STK_SIZE];  // 8 task
TASK_EXTRA_DATA TaskExtraData[8];
TASK_EXTRA_DATA tempTaskExtraData;
INT8U myT, previousTask;
INT8U NumberOfTasks;
INT8U ExecutionTime[8];
INT8U PeriodTime[8];
INT8U TaskNum[8];
INT32U MyStartTime;



/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/
        void  TaskStart(void *data);                  /* Function prototypes of tasks                  */
static  void  TaskStartCreateTasks(void);
static  void  TaskStartDispInit(void);
static  void  TaskStartDisp(void);
        void  TaskClk(void *data);

        void  PeriodicTask(void *data);

/*
*********************************************************************************************************
*                                                  MAIN
*********************************************************************************************************
*/
void main (void)
{
    OS_STK *ptos; //a pointer to the top of stack, OS_STK define in OS_CPU.h (INT32U)
    OS_STK *pbos; //a pointer to the bottom of stack
    INT32U size;

    FILE *InputFile;
    INT8U i;
    InputFile = fopen("Input.txt","r");
    fscanf(InputFile, "%d", &NumberOfTasks);

    for (i = 0; i < NumberOfTasks; i++)
    {
        fscanf(InputFile,"%d%d",&ExecutionTime[i],&PeriodTime[i]);

        TaskExtraData[i].ExecutionTime = ExecutionTime[i] * OS_TICKS_PER_SEC; // tick Rate : 200 Hz (OS_TICKS_PER_SEC = 200)
        TaskExtraData[i].Period = PeriodTime[i] * OS_TICKS_PER_SEC;
        TaskExtraData[i].Deadline = PeriodTime[i] * OS_TICKS_PER_SEC;
        TaskExtraData[i].RemainTime = ExecutionTime[i] * OS_TICKS_PER_SEC;
        TaskNum[i] = i + 1;
    }
    fclose(InputFile);

    PC_DispClrScr(DISP_FGND_WHITE);                        /* Clear the screen                         */

    OSInit();                                              /* Initialize uC/OS-II                      */

    PC_DOSSaveReturn();                                    /* Save environment to return to DOS        */
    PC_VectSet(uCOS, OSCtxSw);                             /* Install uC/OS-II's context switch vector */

    PC_ElapsedInit();                                      /* Initialized elapsed time measurement    PC_ElapasedStart(), PC_ElapseStop() */

    ptos        = &TaskStartStk[TASK_STK_SIZE - 1];        /* TaskStart() will use Floating-Point      */
    pbos        = &TaskStartStk[0];
    size        = TASK_STK_SIZE;
    OSTaskStkInit_FPE_x86(&ptos, &pbos, &size);       // modifiy the top-of-stack pointer
    OSTaskCreateExt(TaskStart, //function pointer
                   (void *)0, // input data
                   ptos,    // pass the new pointer to OSTaskCreateExt() because: OSTaskStkInit_FPE_x86() modifiy the top-of-stack pointer
                   TASK_START_PRIO, //task priority
                   TASK_START_ID, // task ID
                   pbos, //pass the new pointer to OSTaskCreateExt() because: OSTaskStkInit_FPE_x86() modifiy the bottom-of-stack pointer
                   size, //pass the new size to OSTaskCreateExt() because: OSTaskStkInit_FPE_x86() modifiy the size of the stack
                   (void *)0, //task-control-block (TCB) extension pointer (not used)
                   OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR); //tell OSTaskCreateExt : doing stack-size checking, and clear the stack when the task is created

    OSStart();                                             /* Start multitasking                       */
}

/*
*********************************************************************************************************
*                                               STARTUP TASK
*********************************************************************************************************
*/
void TaskStart (void *pdata)
{
#if OS_CRITICAL_METHOD == 3                                /* Allocate storage for CPU status register */
    OS_CPU_SR  cpu_sr;
#endif
    INT16S     key;

    pdata = pdata;                                         /* Prevent compiler warning                 */

    TaskStartDispInit();                                   /* Setup the display                        */

    OS_ENTER_CRITICAL();                                   /* Install uC/OS-II's clock tick ISR        */
    PC_VectSet(0x08, OSTickISR);
    PC_SetTickRate(OS_TICKS_PER_SEC);                      /* Reprogram tick rate                      */
    OS_EXIT_CRITICAL();

    OSStatInit();                                          /* Initialize uC/OS-II's statistics  cpu usage, context switch times       */

    TaskStartCreateTasks();                                /* Create all other tasks                   */

    for (;;) {
        TaskStartDisp();                                   /* Update the display                       */

        if (PC_GetKey(&key)) {                             /* See if key has been pressed              */
            if (key == 0x1B) {                             /* Yes, see if it's the ESCAPE key          */
                PC_DOSReturn();                            /* Yes, return to DOS (terminate emulator)  */
            }
        }

        OSCtxSwCtr = 0;                                    /* Clear context switch counter             */
        OSTimeDly(OS_TICKS_PER_SEC);                       /* Wait one second                          */
    }
}
/*
*********************************************************************************************************
*                                        INITIALIZE THE DISPLAY
*********************************************************************************************************
*/
static  void  TaskStartDispInit (void)
{
    char s[80];
    INT8S i, j;
    INT8U tempPeriodTime, tempExecutionTime, tempTaskNum;

    PC_DispStr( 0,  0, "                            Final Project on uC/OS-II                           ", DISP_FGND_WHITE + DISP_BGND_RED + DISP_BLINK);
    PC_DispStr( 0,  1, "                                  Group 08                                      ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  2, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  3, "                                Scheduling Results                              ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  4, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  5, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  6, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  7, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  8, "Task           Start Time  End Time    Deadline    Period      Excution    Run  ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  9, "-------------  ----------  ----------  ----------  ----------  ----------  ---- ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);


    for (i = 1; i < NumberOfTasks; i++) //insertion sort
    {
        j = i - 1;

        tempPeriodTime      = PeriodTime[i];
        tempExecutionTime   = ExecutionTime[i];
        tempTaskNum 		= TaskNum[i];
        tempTaskExtraData   = TaskExtraData[i];

        while( j >= 0 && PeriodTime[j] > tempPeriodTime)
        {

            PeriodTime[j + 1]    = PeriodTime[j];
            ExecutionTime[j + 1] = ExecutionTime[j];
            TaskNum[j + 1]       = TaskNum[j];
            TaskExtraData[j + 1] = TaskExtraData[j];
            j--;
        }

        PeriodTime[j + 1]    = tempPeriodTime;
        ExecutionTime[j + 1] = tempExecutionTime;
        TaskNum[j + 1]       = tempTaskNum;
        TaskExtraData[j + 1] = tempTaskExtraData;
    }

    for(i=0; i< NumberOfTasks; i++)
    {
        sprintf(s,"Task%3d :                                    %6d     %6d                  ",
            TaskNum[i],
            PeriodTime[i],
            ExecutionTime[i]
		);
        PC_DispStr(0, 10+i, s, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    }
    for(j=(NumberOfTasks+10); j<22; j++)
    {
        PC_DispStr( 0, j, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    }

    PC_DispStr( 0, 22, "#Tasks          :        CPU Usage:     %                                       ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 23, "#Task switch/sec:                                                               ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 24, "                            <-PRESS 'ESC' TO QUIT->                             ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY + DISP_BLINK);
}

/*
*********************************************************************************************************
*                                           UPDATE THE DISPLAY
*********************************************************************************************************
*/
static  void  TaskStartDisp (void)
{
    char   s[80];


    sprintf(s, "%5d", OSTaskCtr);                                  /* Display #tasks running               */
    PC_DispStr(18, 22, s, DISP_FGND_WHITE  + DISP_BGND_BLUE+DISP_BLINK);

    sprintf(s, "%3d", OSCPUUsage);                                 /* Display CPU usage in %               */
    PC_DispStr(36, 22, s, DISP_FGND_WHITE  + DISP_BGND_BLUE+DISP_BLINK);

    sprintf(s, "%5d", OSCtxSwCtr);                                 /* Display #context switches per second */
    PC_DispStr(18, 23, s, DISP_FGND_WHITE  + DISP_BGND_BLUE+DISP_BLINK);

    sprintf(s, "V%4.2f", (float)OSVersion() * 0.01);               /* Display uC/OS-II's version number    */
    PC_DispStr(75, 24, s, DISP_FGND_WHITE  + DISP_BGND_BLUE+DISP_BLINK);

    switch (_8087) {                                               /* Display whether FPU present          */
        case 0:
             PC_DispStr(71, 22, " NO  FPU ", DISP_FGND_YELLOW + DISP_BGND_BLUE);
             break;

        case 1:
             PC_DispStr(71, 22, " 8087 FPU", DISP_FGND_YELLOW + DISP_BGND_BLUE);
             break;

        case 2:
             PC_DispStr(71, 22, "80287 FPU", DISP_FGND_YELLOW + DISP_BGND_BLUE);
             break;

        case 3:
             PC_DispStr(71, 22, "80387 FPU", DISP_FGND_YELLOW + DISP_BGND_BLUE);
             break;
    }
}

/*
*********************************************************************************************************
*                                             CREATE TASKS
*********************************************************************************************************
*/
static void TaskStartCreateTasks(void)
{
	INT8U i;
	char s[80];
	MyStartTime=OSTimeGet();

	OSTaskCreateExt(TaskClk,
		(void *)0,
		&TaskClkStk[TASK_STK_SIZE-1],
		TASK_CLK_PRIO,
		TASK_CLK_ID,
		&TaskClkStk[0],
		TASK_STK_SIZE,
		(void *)0,
		OS_TASK_OPT_STK_CHK|OS_TASK_OPT_STK_CLR
	);

	for(i=0;i<NumberOfTasks;i++)
	{
		OSTaskCreateExt(PeriodicTask, //function pointer
			(void *)0, //input data
			&TaskStk[i][TASK_STK_SIZE-1], //stack top
			(PERIODIC_TASK_START_PRIO+i), //priority
			(PERIODIC_TASK_START_ID+i), //ID
			&TaskStk[i][0], //stack: bottom
			TASK_STK_SIZE,  //stack size
			&TaskExtraData[i], //TCP extension point to taskExtraData[i]
			OS_TASK_OPT_STK_CHK|OS_TASK_OPT_STK_CLR //tell OSTaskCreateExt : doing stack-size checking, and clear the stack when the task is created
		);
	}
}

void  TaskClk (void *data)
{
    char s[40];

    data = data;
    for (;;) {
        PC_GetDateTime(s);
        PC_DispStr(60, 23, s, DISP_FGND_WHITE  + DISP_BGND_BLUE);
        OSTimeDly(OS_TICKS_PER_SEC);
    }
}

/*
*********************************************************************************************************
*                                        Periodic TASK
*********************************************************************************************************
*/
void  PeriodicTask (void *pdata)
{
    INT8U  x, y, now;
    TASK_EXTRA_DATA *MyPtr;
    char s[34];
    char p[34];
    INT8U i;
    INT32U TaskTime;

    pdata=pdata;
    MyPtr = OSTCBCur -> OSTCBExtPtr;
    x = 0;

    MyPtr->Deadline = MyStartTime + MyPtr-> Period;
    MyPtr->RemainTime = MyPtr->ExecutionTime;
    for (;;)
    {
        x++;myT++;
        sprintf(s, "%4d",x);  // number of executions
        PC_DispStr(71, 10 + OSPrioCur - PERIODIC_TASK_START_PRIO, s, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY); //print number of executions

        now = OSTimeGet() / OS_TICKS_PER_SEC - 1;
        sprintf(s, "%10d ",OSTimeGet() / OS_TICKS_PER_SEC - 1);  //start time
        PC_DispStr(14, 10 + OSPrioCur - PERIODIC_TASK_START_PRIO, s, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY); //print start time

        sprintf(s, "%10d ", MyPtr->Deadline / OS_TICKS_PER_SEC - 1); //dead time
        PC_DispStr(34, 10 + OSPrioCur - PERIODIC_TASK_START_PRIO, s, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY); //print dead time

        sprintf(s, "%3d", TaskNum[OSPrioCur-PERIODIC_TASK_START_PRIO]); // task's num
        PC_DispStr(0+3*myT,19,s,DISP_FGND_PURPLE+DISP_BGND_LIGHT_GRAY); //print task's num

        sprintf(s,"%3d",OSTimeGet()/OS_TICKS_PER_SEC - 1); // start time (curr time)
		PC_DispStr(0+3*myT,20,s,DISP_FGND_WHITE+DISP_BGND_LIGHT_GRAY); //print curr time

        previousTask = TaskNum[OSPrioCur-PERIODIC_TASK_START_PRIO];
        y = 0;

        while(1)
        {
            if(MyPtr->RemainTime <=0)
            {
                break;
            }  // remaining execution time decrease in OS_CORE.c OSTimeTick(void)

            if (previousTask != TaskNum[OSPrioCur-PERIODIC_TASK_START_PRIO] && y == 0 && OSTimeGet()/OS_TICKS_PER_SEC - now >= 1)
            {
                y++;
                myT++;
                previousTask == TaskNum[OSPrioCur-PERIODIC_TASK_START_PRIO];
                now = OSTimeGet()/OS_TICKS_PER_SEC;
                sprintf(s, "%3d", TaskNum[OSPrioCur-PERIODIC_TASK_START_PRIO]); // task's num
                PC_DispStr(0+3*myT,19,s,DISP_FGND_PURPLE+DISP_BGND_LIGHT_GRAY); //print task's num

                sprintf(s,"%3d",OSTimeGet()/OS_TICKS_PER_SEC - 1); // start time (curr time)
                PC_DispStr(0+3*myT,20,s,DISP_FGND_WHITE+DISP_BGND_LIGHT_GRAY); //print curr time

            }

            sprintf(s,"%3d",OSTimeGet()/OS_TICKS_PER_SEC - 1); // start time (curr time)
            PC_DispStr(0+3*myT,21,s, DISP_FGND_LIGHT_RED + DISP_BGND_LIGHT_GRAY); //print curr time

        }

        MyPtr->Deadline = MyPtr->Deadline + MyPtr->Period;
        MyPtr->RemainTime = MyPtr->ExecutionTime;

	    sprintf(s,"%10d",OSTimeGet()/OS_TICKS_PER_SEC); // end time (remainTime == 0)
		PC_DispStr(24,10+OSPrioCur-PERIODIC_TASK_START_PRIO,s,DISP_FGND_BLACK+DISP_BGND_LIGHT_GRAY); //print end time

        if(MyPtr->Deadline - MyPtr->Period > OSTimeGet() ) {OSTimeDly(MyPtr->Deadline - MyPtr->Period - OSTimeGet());}
    }
}
