////////////////////////////////////////////////////////////////////////////////
// SoftKinetic DepthSense SDK
//
// COPYRIGHT AND CONFIDENTIALITY NOTICE - SOFTKINETIC CONFIDENTIAL
// INFORMATION
//
// All rights reserved to SOFTKINETIC SENSORS NV (a
// company incorporated and existing under the laws of Belgium, with
// its principal place of business at Boulevard de la Plainelaan 15,
// 1050 Brussels (Belgium), registered with the Crossroads bank for
// enterprises under company number 0811 341 454 - "Softkinetic
// Sensors").
//
// The source code of the SoftKinetic DepthSense Camera Drivers is
// proprietary and confidential information of Softkinetic Sensors NV.
//
// For any question about terms and conditions, please contact:
// info@softkinetic.com Copyright (c) 2002-2012 Softkinetic Sensors NV
////////////////////////////////////////////////////////////////////////////////

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION

#ifdef _MSC_VER
#include <windows.h>
#endif

// Python Module includes
#include <python2.7/Python.h>
#include <python2.7/numpy/arrayobject.h>

// C includes
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

// C++ includes
#include <vector>
#include <exception>
#include <iostream>
#include <thread>

// DepthSense SDK includes
#include <DepthSense.hxx>

using namespace DepthSense;
using namespace std;

static Context g_context;
static DepthNode g_dnode;
static ColorNode g_cnode;
static AudioNode g_anode;

static uint32_t g_aFrames = 0;
static uint32_t g_cFrames = 0;
static uint32_t g_dFrames = 0;

static bool g_bDeviceFound = false;

static ProjectionHelper* g_pProjHelper = NULL;
static StereoCameraParameters g_scp;

static int32_t dW = 320;
static int32_t dH = 240;

int child_pid = 0;

//TODO: BUILD LOCK FOR MAP
// Build lock and two shared mem regions
pthread_mutex_t *lock;
pthread_mutexattr_t matr;

static uint16_t *depthMap;
static uint16_t depthMapClone[320*240];
int shmsz = sizeof(uint16_t)*dW*dH;

/*----------------------------------------------------------------------------*/
// New audio sample event handler
static void onNewAudioSample(AudioNode node, AudioNode::NewSampleReceivedData data)
{
    //printf("A#%u: %d\n",g_aFrames,data.audioData.size());
    g_aFrames++;
}



/*----------------------------------------------------------------------------*/
// New color sample event handler
static void onNewColorSample(ColorNode node, ColorNode::NewSampleReceivedData data)
{
    //printf("C#%u: %d\n",g_cFrames,data.colorMap.size());
    g_cFrames++;
}

/*----------------------------------------------------------------------------*/
// New depth sample event handler
static void onNewDepthSample(DepthNode node, DepthNode::NewSampleReceivedData data)
{

    //pthread_mutex_lock(lock);
    for (int i=0; i<dH; i++) {
        for(int j=0; j<dW; j++) {
            depthMap[i*dW +j] = (data.depthMap[i*dW + j]); 
        }
    }
    //pthread_mutex_unlock(lock);
    g_dFrames++;
}

/*----------------------------------------------------------------------------*/
static void configureAudioNode()
{
    g_anode.newSampleReceivedEvent().connect(&onNewAudioSample);

    AudioNode::Configuration config = g_anode.getConfiguration();
    config.sampleRate = 44100;

    try 
    {
        g_context.requestControl(g_anode,0);

        g_anode.setConfiguration(config);
        
        g_anode.setInputMixerLevel(0.5f);
    }
    catch (ArgumentException& e)
    {
        printf("Argument Exception: %s\n",e.what());
    }
    catch (UnauthorizedAccessException& e)
    {
        printf("Unauthorized Access Exception: %s\n",e.what());
    }
    catch (ConfigurationException& e)
    {
        printf("Configuration Exception: %s\n",e.what());
    }
    catch (StreamingException& e)
    {
        printf("Streaming Exception: %s\n",e.what());
    }
    catch (TimeoutException&)
    {
        printf("TimeoutException\n");
    }
}

/*----------------------------------------------------------------------------*/
static void configureDepthNode()
{
    g_dnode.newSampleReceivedEvent().connect(&onNewDepthSample);

    DepthNode::Configuration config = g_dnode.getConfiguration();
    config.frameFormat = FRAME_FORMAT_QVGA;
    config.framerate = 25;
    config.mode = DepthNode::CAMERA_MODE_CLOSE_MODE;
    config.saturation = true;

    g_dnode.setEnableDepthMap(true);

    try 
    {
        g_context.requestControl(g_dnode,0);

        g_dnode.setConfiguration(config);
    }
    catch (ArgumentException& e)
    {
        printf("Argument Exception: %s\n",e.what());
    }
    catch (UnauthorizedAccessException& e)
    {
        printf("Unauthorized Access Exception: %s\n",e.what());
    }
    catch (IOException& e)
    {
        printf("IO Exception: %s\n",e.what());
    }
    catch (InvalidOperationException& e)
    {
        printf("Invalid Operation Exception: %s\n",e.what());
    }
    catch (ConfigurationException& e)
    {
        printf("Configuration Exception: %s\n",e.what());
    }
    catch (StreamingException& e)
    {
        printf("Streaming Exception: %s\n",e.what());
    }
    catch (TimeoutException&)
    {
        printf("TimeoutException\n");
    }

}

/*----------------------------------------------------------------------------*/
static void configureColorNode()
{
    // connect new color sample handler
    g_cnode.newSampleReceivedEvent().connect(&onNewColorSample);

    ColorNode::Configuration config = g_cnode.getConfiguration();
    config.frameFormat = FRAME_FORMAT_VGA;
    config.compression = COMPRESSION_TYPE_MJPEG;
    config.powerLineFrequency = POWER_LINE_FREQUENCY_50HZ;
    config.framerate = 25;

    g_cnode.setEnableColorMap(true);

    try 
    {
        g_context.requestControl(g_cnode,0);

        g_cnode.setConfiguration(config);
    }
    catch (ArgumentException& e)
    {
        printf("Argument Exception: %s\n",e.what());
    }
    catch (UnauthorizedAccessException& e)
    {
        printf("Unauthorized Access Exception: %s\n",e.what());
    }
    catch (IOException& e)
    {
        printf("IO Exception: %s\n",e.what());
    }
    catch (InvalidOperationException& e)
    {
        printf("Invalid Operation Exception: %s\n",e.what());
    }
    catch (ConfigurationException& e)
    {
        printf("Configuration Exception: %s\n",e.what());
    }
    catch (StreamingException& e)
    {
        printf("Streaming Exception: %s\n",e.what());
    }
    catch (TimeoutException&)
    {
        printf("TimeoutException\n");
    }
}

/*----------------------------------------------------------------------------*/
static void configureNode(Node node)
{
    if ((node.is<DepthNode>())&&(!g_dnode.isSet()))
    {
        g_dnode = node.as<DepthNode>();
        configureDepthNode();
        g_context.registerNode(node);
    }

    if ((node.is<ColorNode>())&&(!g_cnode.isSet()))
    {
        g_cnode = node.as<ColorNode>();
        configureColorNode();
        g_context.registerNode(node);
    }

    if ((node.is<AudioNode>())&&(!g_anode.isSet()))
    {
        g_anode = node.as<AudioNode>();
        configureAudioNode();
        g_context.registerNode(node);
    }
}

/*----------------------------------------------------------------------------*/
static void onNodeConnected(Device device, Device::NodeAddedData data)
{
    configureNode(data.node);
}

/*----------------------------------------------------------------------------*/
static void onNodeDisconnected(Device device, Device::NodeRemovedData data)
{
    if (data.node.is<AudioNode>() && (data.node.as<AudioNode>() == g_anode))
        g_anode.unset();
    if (data.node.is<ColorNode>() && (data.node.as<ColorNode>() == g_cnode))
        g_cnode.unset();
    if (data.node.is<DepthNode>() && (data.node.as<DepthNode>() == g_dnode))
        g_dnode.unset();
    printf("Node disconnected\n");
}

/*----------------------------------------------------------------------------*/
static void onDeviceConnected(Context context, Context::DeviceAddedData data)
{
    if (!g_bDeviceFound)
    {
        data.device.nodeAddedEvent().connect(&onNodeConnected);
        data.device.nodeRemovedEvent().connect(&onNodeDisconnected);
        g_bDeviceFound = true;
    }
}

/*----------------------------------------------------------------------------*/
static void onDeviceDisconnected(Context context, Context::DeviceRemovedData data)
{
    g_bDeviceFound = false;
    printf("Device disconnected\n");
}

extern "C" {
    static void killds(){
        if (child_pid !=0)
            kill(child_pid, SIGTERM);
            munmap(depthMap, shmsz);
            munmap(lock, sizeof(pthread_mutex_t));

    }
}

static void initds()
{
    // shared mem like a pro.
    if ((depthMap = (uint16_t *) mmap(NULL, shmsz, PROT_READ|PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED) {
        perror("mmap: cannot alloc shmem;"); 
        exit(1); 
    }
    if ((lock = (pthread_mutex_t *) mmap(NULL, sizeof(pthread_mutex_t), PROT_READ|PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED) {
        perror("mmap: cannot alloc shmem;"); 
        exit(1); 
    }

    pthread_mutexattr_init(&matr);

    // child goes into loop
    child_pid = fork();
    if (child_pid == 0) {
        g_context = Context::create("localhost");
        g_context.deviceAddedEvent().connect(&onDeviceConnected);
        g_context.deviceRemovedEvent().connect(&onDeviceDisconnected);

        // Get the list of currently connected devices
        vector<Device> da = g_context.getDevices();

        // We are only interested in the first device
        if (da.size() >= 1)
        {
            g_bDeviceFound = true;

            da[0].nodeAddedEvent().connect(&onNodeConnected);
            da[0].nodeRemovedEvent().connect(&onNodeDisconnected);

            vector<Node> na = da[0].getNodes();
            
            //printf("Found %u nodes\n",na.size());
            
            for (int n = 0; n < (int)na.size();n++)
                configureNode(na[n]);
        }

        g_context.startNodes();
        g_context.run();
        g_context.stopNodes();

        if (g_cnode.isSet()) g_context.unregisterNode(g_cnode);
        if (g_dnode.isSet()) g_context.unregisterNode(g_dnode);
        if (g_anode.isSet()) g_context.unregisterNode(g_anode);

        if (g_pProjHelper)
            delete g_pProjHelper;

        exit(EXIT_SUCCESS);
    }

}
/*----------------------------------------------------------------------------*/

static PyObject *getDepth(PyObject *self, PyObject *args) 
{
    int depthWidth = 320;
    int depthHeight = 240;
    npy_intp dims[2] = {depthHeight, depthWidth};

    //pthread_mutex_lock(lock);
    memcpy(depthMapClone, depthMap, shmsz);
    //pthread_mutex_unlock(lock);
    return PyArray_SimpleNewFromData(2, dims, NPY_UINT16, depthMapClone);
}

static PyObject *initDepth(PyObject *self, PyObject *args)
{
    initds();
    return Py_None;
}

static PyObject *killDepth(PyObject *self, PyObject *args)
{
    killds();
    return Py_None;
}

static PyMethodDef DepthSenseMethods[] = {
    {"getDepthMap",  getDepth, METH_VARARGS, "Get Depth Map"},
    {"initDepthSense",  initDepth, METH_VARARGS, "Init DepthSense"},
    {"killDepthSense",  killDepth, METH_VARARGS, "Kill DepthSense"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};


PyMODINIT_FUNC initDepthSense(void)
{
    (void) Py_InitModule("DepthSense", DepthSenseMethods);
    (void) Py_AtExit(killds);
    import_array();
}

int main(int argc, char* argv[])
{
    
    /* Pass argv[0] to the Python interpreter */
    Py_SetProgramName((char *)"DepthSense");

    /* Initialize the Python interpreter.  Required. */
    Py_Initialize();

    /* Add a static module */
    initDepthSense();

    //thread worker(&init);
    //printf("HERE\n");
    //worker.join();
    
    return 0;
}
