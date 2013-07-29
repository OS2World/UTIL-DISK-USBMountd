/* -------------------------------------------------------------------------- */
#define INCL_DOSPROCESS       /* DOS process values (for DosZ) */
#define INCL_DOSEXCEPTIONS    /* DOS exception values */
#define INCL_ERRORS           /* DOS error values     */
#define INCL_DOSSEMAPHORES
#define INCL_DOSMISC
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "./h/getopt.h"
#include "./h/lvm_intr.h"
#include "./h/usbcalls.h"
void RefreshLVM(void);
void writelog(char *text,char *pipeName);
BOOL term=FALSE,nosound=FALSE,checkdrives=FALSE,startupcheck=FALSE;
PCHAR logname;
ULONG _System MyTermHandler( PEXCEPTIONREPORTRECORD       p1,
                             PEXCEPTIONREGISTRATIONRECORD p2,
                             PCONTEXTRECORD               p3,
                             PVOID                        pv );

/* ------------------------------------------------------------------------- */
int main( int argc, char *argv[] )
{
 HEV hDevPluged;   // Event Set When an USB dev pluged in;
 HEV hDevUnpluged; // Event Set When an USB dev is Unpluged
 USBNOTIFY NotifyID; // Id Returned from UsbRegisterDeviceNotification
 INT opt,dsleep=600,sleep=dsleep;
 EXCEPTIONREGISTRATIONRECORD  RegRec  = {0};
 APIRET rc;
 USBHANDLE hUSBDevice;
 ULONG NumDevices;
 ULONG   ulPostCt;
 ULONG NumDevicesLast = 0;
 UCHAR ucBuffer[640];

/* Adding MyTermHandler */
RegRec.ExceptionHandler = (ERR)MyTermHandler;
rc = DosSetExceptionHandler( &RegRec );
if (rc != NO_ERROR) {
   fprintf(stderr,"ERROR: DosSetExceptionHandler error: return code = %u\n",rc);
   return 1;
}

// checking for driver presence
rc = UsbQueryNumberDevices(&NumDevices);
if (rc!=0) {
 fprintf(stderr,"ERROR: can`t query number devices, rc=%d",rc);
 return 1;
}

printf("USBMOUNTD version 0.0.2\n");
  while ((opt = getopt(argc, argv, "?nsl:t:c")) != EOF)
    switch (opt)
    {
    case '?':
       printf ("\rUSAGE: usbmountd.exe [-n] [-l logname] [-t msec] [-s] [-?]\n");
       printf ("       -n      dont beep in speaker when device added\n");
       printf ("       -s      refresh LVM at startup\n");
       printf ("       -l      log filename (can be a pipe)\n");
       printf ("       -t      time to wait after device detection (default=%dms)\n",dsleep);
       printf ("       -c      automatic check mounted device if it has dirty flag.\n");
       printf ("       -?      show this help\n");
       return(0);
       break;
    case 'n':
       nosound=TRUE;
       break;
    case 'l':
       logname=optarg;
       break;
    case 'c':
       checkdrives=TRUE;
       break;
    case 't':
       sleep=atol(optarg);
       break;
    case 's':
     startupcheck=TRUE;
     break;
    }
if (NumDevices>0 && startupcheck==TRUE) RefreshLVM();
rc=DosCreateEventSem(NULL,&hDevPluged,
                     DCE_AUTORESET|DC_SEM_SHARED,FALSE);
if(rc) {fprintf(stderr,"ERROR: Can`t create event sem.\n");return(1);}
DosCreateEventSem(NULL,&hDevUnpluged,DCE_AUTORESET,FALSE);
rc= UsbRegisterChangeNotification( &NotifyID, hDevPluged,hDevUnpluged);
writelog("USBMOUNTD started. Waitig for the event.\n",logname);
for (;;)
{
 DosWaitEventSem(hDevPluged,(ULONG) SEM_INDEFINITE_WAIT);
 if (term) break;
 DosSleep(sleep);
 RefreshLVM();
}
/* exiting */
rc=UsbDeregisterNotification(NotifyID);
DosCloseEventSem(hDevUnpluged);
DosCloseEventSem(hDevPluged);
return(0);
}

void RefreshLVM()
{
    CARDINAL32 Error_Code = 1;
    ULONG    DriveMapOld;
    ULONG    DriveMapNew;
    ULONG    start = 3;                  /* Initial disk num           */
    ULONG    dnum;                       /* Disk num variable          */
    CHAR     DeviceName[4];
    CHAR     ChkString[200];
    FILE *fp1,*fp2;
    FILESTATUS3  PathInfo = {{0}};       /* Buffer for path information */
    ULONG        BufSize     = sizeof(FILESTATUS3);  /* Size of above buffer */

    writelog("Refreshing LVM.\n",logname);
    if (nosound==FALSE) {
      DosBeep(300,100);
      DosBeep(100,100);
    }
    DosQueryCurrentDisk(NULL, &DriveMapOld);
    Rediscover_PRMs( &Error_Code );
    if ( Error_Code != LVM_ENGINE_NO_ERROR ) {
        fprintf(stderr,"ERROR: Rediscover_PRMs returned %d\n", Error_Code );
    }
    DosQueryCurrentDisk(NULL, &DriveMapNew);
    DriveMapNew>>=start-1;           /* Shift to the first drive   */
    DriveMapOld>>=start-1;
    for (dnum = start; dnum <= 26; dnum++)
        {
         if (DriveMapNew&1 && (DriveMapOld&1)==0) {
          sprintf(DeviceName, "%c:\\", dnum+'A'-1);
          sprintf(ChkString,"Device %s added.\n",DeviceName);
          writelog(ChkString,logname);
          // checking new device
           DosError(FERR_DISABLEHARDERR);
           Error_Code = DosQueryPathInfo(DeviceName, FIL_STANDARD,  &PathInfo, BufSize);
           if (Error_Code!=0 && checkdrives==TRUE)
          {
            sprintf(ChkString,"Checking drive %c: \n",dnum+'A'-1);
            writelog(ChkString,logname);
            sprintf(ChkString,"chkdsk %c: /f >nul 2>nul",dnum+'A'-1);
            Error_Code = system(ChkString);
            if(Error_Code) {
                 sprintf(ChkString,"ERROR: chkdsk %c: /f returned rc %d\n",
                 dnum+'A'-1,Error_Code);
                }
            else
          {
                 sprintf(ChkString,"Drive %c: checked. \n",
                 dnum+'A'-1);
            }
                 writelog(ChkString,logname);

          }
         }
          DriveMapNew>>=1;          /* Shift to the next drive    */
          DriveMapOld>>=1;
        }
 return;

    return;
}

ULONG _System MyTermHandler( PEXCEPTIONREPORTRECORD       p1,
                             PEXCEPTIONREGISTRATIONRECORD p2,
                             PCONTEXTRECORD               p3,
                             PVOID                        pv )
{
  APIRET   rc  = NO_ERROR;   /* Return code */
  if (p1->ExceptionNum == XCPT_SIGNAL) {
        rc = DosAcknowledgeSignalException ( XCPT_SIGNAL_BREAK );
        if (rc != NO_ERROR) {
          fprintf(stderr,"ERROR: DosAcknowledgeSignalException: return code = %u\n", rc);
          return 1;
        } else {
          printf("Terminating program\n");
          term=TRUE;
          return XCPT_CONTINUE_EXECUTION;  /* cont. execution */
        } /* endif */
  }
  return XCPT_CONTINUE_SEARCH;       /* Exception not resolved. */
}

 void writelog(char *text,char *pipeName)
 {
    CHAR buf[200];
    FILE *f;
    if (pipeName!=NULL)
    {
    sprintf(buf,"%.6s",pipeName);
    if (0 == strcmpi(buf,"\\PIPE\\")) f = fopen(pipeName,"w");
    else f = fopen(pipeName,"a");
    if(f)
    {
       fprintf(f,"%s",text);
       fclose(f);
    }
    }
    printf(text);
 }
