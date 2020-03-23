//
// Created by rverst on 2020-03-20.
//

#include "gstPipeline.h"
#include "glDisplay.h"
#include "commandLine.h"

#include <signal.h>


bool signal_recieved = false;

void sig_handler(int signo)
{
	if( signo == SIGINT )
	{
		printf("received SIGINT\n");
		signal_recieved = true;
	}
}


int main( int argc, char** argv )
{
	commandLine cmdLine(argc, argv);

	/*
	 * attach signal handler
	 */
	if( signal(SIGINT, sig_handler) == SIG_ERR )
		printf("\ncan't catch SIGINT\n");

    /*
     * create the pipeline
     * e.g: "rtspsrc location=rtsp://user:pw@192.168.0.170/Streaming/Channels/1 ! queue ! rtph264depay ! h264parse ! queue ! omxh264dec ! appsink name=mysink"
     */
    gstPipeline* pipeline = gstPipeline::Create(
            cmdLine.GetString("pipeline"),
            cmdLine.GetInt("width", gstPipeline::DefaultWidth),
            cmdLine.GetInt("height", gstPipeline::DefaultHeight),
            cmdLine.GetInt("depth", gstPipeline::DefaultDepth));


    if( !pipeline )
    {
        printf("\ngst-pipeline:  failed to initialize gstreamer pipeline\n");
        return 0;
    }

    printf("\ngst-pipeline:  successfully initialized video device\n");
    printf("    width:  %u\n", pipeline->GetWidth());
    printf("   height:  %u\n", pipeline->GetHeight());
    printf("    depth:  %u (bpp)\n", pipeline->GetPixelDepth());


    /*
     * create openGL window
     */
    glDisplay* display = glDisplay::Create();

    if( !display )
        printf("gst-pipeline:  failed to create openGL display\n");


    /*
     * start streaming
     */
    if( !pipeline->Open() )
    {
        printf("gst-pipeline:  failed to open pipeline for streaming\n");
        return 0;
    }

    printf("\ngst-pipeline:  pipeline open for streaming\n");


    /*
     * processing loop
     */
    while( !signal_recieved )
    {
        // capture latest image
        float* imgRGBA = NULL;

        if( !pipeline->CaptureRGBA(&imgRGBA, 1000) )
            printf("gst-pipeline:  failed to capture RGBA image\n");

        // update display
        if( display != NULL )
        {
            display->RenderOnce((float*)imgRGBA, pipeline->GetWidth(), pipeline->GetHeight());

            // update status bar
            char str[256];
            sprintf(str, "gStreamer pipeline (%ux%u) | %.0f FPS", pipeline->GetWidth(), pipeline->GetHeight(), display->GetFPS());
            display->SetTitle(str);

            // check if the user quit
            if( display->IsClosed() )
                signal_recieved = true;
        }
    }


    /*
     * destroy resources
     */
    printf("\ngst-pipeline:  shutting down...\n");

    SAFE_DELETE(pipeline);
    SAFE_DELETE(display);

    printf("gst-pipeline:  shutdown complete.\n");
    return 0;
}
