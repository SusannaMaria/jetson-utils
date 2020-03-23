/*
 * Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "PyPipeline.h"
#include "PyCUDA.h"

#include "gstPipeline.h"


// PyPipeline container
typedef struct {
    PyObject_HEAD
    gstPipeline* pipeline;
} PyPipeline_Object;


// New
static PyObject* PyPipeline_New( PyTypeObject *type, PyObject *args, PyObject *kwds )
{
	printf(LOG_PY_UTILS "PyPipeline_New()\n");
	
	// allocate a new container
	PyPipeline_Object* self = (PyPipeline_Object*)type->tp_alloc(type, 0);
	
	if( !self )
	{
		PyErr_SetString(PyExc_MemoryError, LOG_PY_UTILS "gstPipeline tp_alloc() failed to allocate a new object");
		printf(LOG_PY_UTILS "gstPipeline tp_alloc() failed to allocate a new object\n");
		return NULL;
	}
	
    self->pipeline = NULL;
    return (PyObject*)self;
}


// Init
static int PyPipeline_Init( PyPipeline_Object* self, PyObject *args, PyObject *kwds )
{
	printf(LOG_PY_UTILS "PyPipeline_Init()\n");
	
	// parse arguments
    const char* strPipeline = NULL;
    int image_width   = gstPipeline::DefaultWidth;
    int image_height  = gstPipeline::DefaultHeight;
    int pixel_depth  = gstPipeline::DefaultDepth;

	static char* kwlist[] = {"pipeline", "width", "height", "depth", NULL};

	if( !PyArg_ParseTupleAndKeywords(args, kwds, "|iis", kwlist, &strPipeline, &image_width, &image_height, &pixel_depth))
	{
		PyErr_SetString(PyExc_Exception, LOG_PY_UTILS "gstPipeline.__init()__ failed to parse args tuple");
		printf(LOG_PY_UTILS "gstPipeline.__init()__ failed to parse args tuple\n");
		return -1;
	}
  
	if( image_width <= 0 )	
		image_width = gstPipeline::DefaultWidth;

	if( image_height <= 0 )	
		image_height = gstPipeline::DefaultHeight;

	// create the pipeline object
	gstPipeline* pipeline = gstPipeline::Create(strPipeline, image_width, image_height, pixel_depth);

	if( !pipeline )
	{
		PyErr_SetString(PyExc_Exception, LOG_PY_UTILS "failed to create gstPipeline device");
		return -1;
	}

	self->pipeline = pipeline;
	return 0;
}


// Deallocate
static void PyPipeline_Dealloc( PyPipeline_Object* self )
{
	printf(LOG_PY_UTILS "PyPipeline_Dealloc()\n");

	// free the network
	if( self->pipeline != NULL )
	{
		delete self->pipeline;
		self->pipeline = NULL;
	}
	
	// free the container
	Py_TYPE(self)->tp_free((PyObject*)self);
}


// Open
static PyObject* PyPipeline_Open( PyPipeline_Object* self )
{
	if( !self || !self->pipeline )
	{
		PyErr_SetString(PyExc_Exception, LOG_PY_UTILS "gstPipeline invalid object instance");
		return NULL;
	}

	if( !self->pipeline->Open() )
	{
		PyErr_SetString(PyExc_Exception, LOG_PY_UTILS "failed to open gstPipeline device for streaming");
		return NULL;
	}

	Py_RETURN_NONE; 
}


// Close
static PyObject* PyPipeline_Close( PyPipeline_Object* self )
{
	if( !self || !self->pipeline )
	{
		PyErr_SetString(PyExc_Exception, LOG_PY_UTILS "gstPipeline invalid object instance");
		return NULL;
	}

	self->pipeline->Close();
	Py_RETURN_NONE; 
}


// CaptureRGBA
static PyObject* PyPipeline_CaptureRGBA( PyPipeline_Object* self, PyObject* args, PyObject* kwds )
{
	if( !self || !self->pipeline )
	{
		PyErr_SetString(PyExc_Exception, LOG_PY_UTILS "gstPipeline invalid object instance");
		return NULL;
	}

	// parse arguments
	int pyTimeout  = -1;
	int pyZeroCopy = 0;

	static char* kwlist[] = {"timeout", "zeroCopy", NULL};

	if( !PyArg_ParseTupleAndKeywords(args, kwds, "|ii", kwlist, &pyTimeout, &pyZeroCopy))
	{
		PyErr_SetString(PyExc_Exception, LOG_PY_UTILS "gstPipeline.CaptureRGBA() failed to parse args tuple");
		return NULL;
	}

	// convert signed timeout to unsigned long
	uint64_t timeout = UINT64_MAX;

	if( pyTimeout >= 0 )
		timeout = pyTimeout;

	// convert int zeroCopy to boolean
	const bool zeroCopy = pyZeroCopy <= 0 ? false : true;

	// capture RGBA
	float* ptr = NULL;

	if( !self->pipeline->CaptureRGBA(&ptr, timeout, zeroCopy) )
	{
		PyErr_SetString(PyExc_Exception, LOG_PY_UTILS "gstPipeline failed to CaptureRGBA()");
		return NULL;
	}

	// register memory capsule (gstPipeline will free the underlying memory when pipeline is deleted)
	PyObject* capsule = zeroCopy ? PyCUDA_RegisterMappedMemory(ptr, ptr, false) : PyCUDA_RegisterMemory(ptr, false);

	if( !capsule )
		return NULL;

	// create dimension objects
	PyObject* pyWidth  = PYLONG_FROM_LONG(self->pipeline->GetWidth());
	PyObject* pyHeight = PYLONG_FROM_LONG(self->pipeline->GetHeight());

	// return tuple
	PyObject* tuple = PyTuple_Pack(3, capsule, pyWidth, pyHeight);

	Py_DECREF(capsule);
	Py_DECREF(pyWidth);
	Py_DECREF(pyHeight);

	return tuple;
}


// GetWidth()
static PyObject* PyPipeline_GetWidth( PyPipeline_Object* self )
{
	if( !self || !self->pipeline )
	{
		PyErr_SetString(PyExc_Exception, LOG_PY_UTILS "gstPipeline invalid object instance");
		return NULL;
	}

	return PYLONG_FROM_UNSIGNED_LONG(self->pipeline->GetWidth());
}


// GetHeight()
static PyObject* PyPipeline_GetHeight( PyPipeline_Object* self )
{
	if( !self || !self->pipeline )
	{
		PyErr_SetString(PyExc_Exception, LOG_PY_UTILS "gstPipeline invalid object instance");
		return NULL;
	}

	return PYLONG_FROM_UNSIGNED_LONG(self->pipeline->GetHeight());
}



//-------------------------------------------------------------------------------
static PyTypeObject pyPipeline_Type =
{
    PyVarObject_HEAD_INIT(NULL, 0)
};

static PyMethodDef pyPipeline_Methods[] =
{
	{ "Open", (PyCFunction)PyPipeline_Open, METH_NOARGS, "Open the pipeline for streaming frames"},
	{ "Close", (PyCFunction)PyPipeline_Close, METH_NOARGS, "Stop streaming pipeline frames"},
	{ "CaptureRGBA", (PyCFunction)PyPipeline_CaptureRGBA, METH_VARARGS|METH_KEYWORDS, "Capture a pipeline frame and convert it to float4 RGBA"},
	{ "GetWidth", (PyCFunction)PyPipeline_GetWidth, METH_NOARGS, "Return the width of the pipeline (in pixels)"},
	{ "GetHeight", (PyCFunction)PyPipeline_GetHeight, METH_NOARGS, "Return the height of the pipeline (in pixels)"},
	{NULL}  /* Sentinel */
};

// Register types
bool PyPipeline_RegisterTypes( PyObject* module )
{
	if( !module )
		return false;

    pyPipeline_Type.tp_name 	  = PY_UTILS_MODULE_NAME ".gstPipeline";
    pyPipeline_Type.tp_basicsize = sizeof(PyPipeline_Object);
    pyPipeline_Type.tp_flags 	  = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    pyPipeline_Type.tp_methods   = pyPipeline_Methods;
    pyPipeline_Type.tp_new 	  = PyPipeline_New;
    pyPipeline_Type.tp_init	  = (initproc)PyPipeline_Init;
    pyPipeline_Type.tp_dealloc	  = (destructor)PyPipeline_Dealloc;
    pyPipeline_Type.tp_doc  	  = "Generic pipeline using GStreamer";
	 
	if( PyType_Ready(&pyPipeline_Type) < 0 )
	{
		printf(LOG_PY_UTILS "gstPipeline PyType_Ready() failed\n");
		return false;
	}
	
	Py_INCREF(&pyPipeline_Type);
    
	if( PyModule_AddObject(module, "gstPipeline", (PyObject*)&pyPipeline_Type) < 0 )
	{
		printf(LOG_PY_UTILS "gstPipeline PyModule_AddObject('gstPipeline') failed\n");
		return false;
	}

	return true;
}

static PyMethodDef pyPipeline_Functions[] =
{
	{NULL}  /* Sentinel */
};

// Register functions
PyMethodDef* PyPipeline_RegisterFunctions()
{
	return pyPipeline_Functions;
}
