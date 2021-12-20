#include <cstdio>
#include "westeros-gl.h"
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>
#include <chrono>

static WstGLCtx *ctx = 0;
static WstGLCtx *wCtx= 0;
static int wDisplayWidth;
static int wDisplayHeight;


void displaySizeCallback( void *userData, int width, int height )
{
   WstGLCtx *wCtx= (WstGLCtx*)userData;

   if ( (wDisplayWidth != width) || (wDisplayHeight != height) )
   {
      printf("\nwesteros_TDKHAL: display size changed: %dx%d\n", width, height);
      wDisplayWidth= width;
      wDisplayHeight= height;
   }
}


int main()
{
   void *nativeWindow = 0;
   ctx = WstGLInit();
   if (ctx)
   {
     printf("\nGot ctx , WstGLInit SUCCESS\n");
     int width, height, x, y, w, h;
     unsigned int caps = 0;
     char mode[20];
     printf("\nCreating Native Window for 1280x720\n");
     nativeWindow = WstGLCreateNativeWindow( ctx, 0, 0, 1280, 720 );

     if (nativeWindow)
     {
	printf("\nGot nativeWindow, WstGLCreateNativeWindow SUCCESS\n");
	WstGLDisplayInfo displayInfo;
	if ( WstGLGetDisplayInfo( ctx, &displayInfo ) )
	{
	   printf("\nGot displayInfo, WstGLDisplayInfo SUCCESS\n");
	   printf("\ndisplayInfo Values width=%d height=%d \n", displayInfo.width, displayInfo.height );
           wDisplayWidth = displayInfo.width;
	   wDisplayHeight = displayInfo.height;

	   if ( WstGLGetDisplaySafeArea( ctx , &x, &y, &w, &h ) )
	   {
	      printf("\nGot DisplaySafeArea, WstGLGetDisplaySafeArea SUCCESS\n");
	      printf("\nDisplaySafeArea x=%d, y=%d, w=%d, h=%d", x, y, w, h);
	   }
	   else
           {
	      printf("\nWstGLGetDisplaySafeArea FAILED\n");
	   }

	   WstGLAddDisplaySizeListener( ctx, ctx, displaySizeCallback);
	   const char *env= getenv( "SLEEP_LEVEL" );
	   int sleepLevel =10;
           if ( env )
           {
              sleepLevel= atoi( env );
           }
	   auto start = std::chrono::steady_clock::now();
	   if ( sleepLevel == 0)
	   {
	      printf("\nWaiting indefinitely\n");
	   }
	   else
           {
	      printf("\nWaiting for %d seconds in chrono loop\n",sleepLevel);
	   }
	   while(1)
	   {
	      if((sleepLevel !=0) && (std::chrono::steady_clock::now() - start > std::chrono::seconds(sleepLevel)))
	        break;
	   }

	   goto exit;
	}
	else
	{
	   printf("\nWstGLDisplayInfo FAILED\n");
	}
     }
     else
     {
	printf("\nWstGLCreateNativeWindow FAILED\n");
     }
   }
   else
   {
     printf("\nWstGLInit FAILED\n");
   }

   exit:
       WstGLRemoveDisplaySizeListener( ctx, displaySizeCallback);
       WstGLDestroyNativeWindow( ctx, (void*)nativeWindow );
       WstGLTerm( ctx );

   return 0;
}
