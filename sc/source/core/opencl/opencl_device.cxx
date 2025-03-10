/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifdef _WIN32
#include <windows.h>
#elif defined __MACH__
#include <mach/mach_time.h>
#else
#include <sys/time.h>
#endif
#include <time.h>
#include <math.h>
#include <float.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <boost/scoped_ptr.hpp>

#include "opencl_device.hxx"

#define INPUTSIZE  15360
#define OUTPUTSIZE 15360

#define STRINGIFY(...) #__VA_ARGS__"\n"
//#define LOG_PRINTF(x) (std::cout << x << std::endl)
#define LOG_PRINTF(x)

#define DS_CHECK_STATUS(status, name) \
    if (CL_SUCCESS != status) \
    { \
        LOG_PRINTF("[OCL] Error code is " << status << " at " << name); \
    }

namespace sc { namespace OpenCLDevice {

bool bIsInited = false;
bool bIsDeviceSelected = false;
ds_device selectedDevice;

struct LibreOfficeDeviceScore
{
    double fTime;     // small time means faster device
    bool bNoCLErrors; // were there any opencl errors
};

struct LibreOfficeDeviceEvaluationIO
{
    std::vector<double> input0;
    std::vector<double> input1;
    std::vector<double> input2;
    std::vector<double> input3;
    std::vector<double> output;
    unsigned long inputSize;
    unsigned long outputSize;
};

struct timer
{
#ifdef _WIN32
    LARGE_INTEGER start;
#else
    long long start;
#endif
};

const char* source = STRINGIFY(
\n#if defined(KHR_DP_EXTENSION)
\n#pragma OPENCL EXTENSION cl_khr_fp64 : enable
\n#elif defined(AMD_DP_EXTENSION)
\n#pragma OPENCL EXTENSION cl_amd_fp64 : enable
\n#endif
    \n
    int isNan(fp_t a) { return a != a; }
    fp_t fsum(fp_t a, fp_t b) { return a + b; }

    fp_t fAverage(__global fp_t* input)
{
    fp_t sum = 0;
    int count = 0;
    for (int i = 0; i < INPUTSIZE; i++)
    {
        if (!isNan(input[i]))
        {
            sum = fsum(input[i], sum);
            count += 1;
        }
    }
    return sum / (fp_t)count;
}
    fp_t fMin(__global fp_t* input)
{
    fp_t min = MAXFLOAT;
    for (int i = 0; i < INPUTSIZE; i++)
    {
        if (!isNan(input[i]))
        {
            min = fmin(input[i], min);
        }
    }
    return min;
}
    fp_t fSoP(__global fp_t* input0, __global fp_t* input1)
{
    fp_t sop = 0.0;
    for (int i = 0; i < INPUTSIZE; i++)
    {
        sop += (isNan(input0[i]) ? 0 : input0[i]) * (isNan(input1[i]) ? 0 : input1[i]);
    }
    return sop;
}
    __kernel void DynamicKernel(
        __global fp_t* result, __global fp_t* input0, __global fp_t* input1, __global fp_t* input2, __global fp_t* input3)
{
    int gid0 = get_global_id(0);
    fp_t tmp0 = fAverage(input0);
    fp_t tmp1 = fMin(input1) * fSoP(input2, input3);
    result[gid0] = fsum(tmp0, tmp1);
}
    );

size_t sourceSize[] = { strlen(source) };

/*************************************************************************/
/* INTERNAL FUNCTIONS                                                    */
/*************************************************************************/
/* Timer functions - start timer */
void timerStart(timer* mytimer)
{
#ifdef _WIN32
    QueryPerformanceCounter(&mytimer->start);
#elif defined __MACH__
    mytimer->start = mach_absolute_time();
#else
    struct timespec s;
    clock_gettime(CLOCK_MONOTONIC, &s);
    mytimer->start = (long long)s.tv_sec * (long long)1.0E6 + (long long)s.tv_nsec / (long long)1.0E3;
#endif
}

/* Timer functions - get current value */
double timerCurrent(timer* mytimer)
{
#ifdef _WIN32
    LARGE_INTEGER stop, frequency;
    QueryPerformanceCounter(&stop);
    QueryPerformanceFrequency(&frequency);
    double time = ((double)(stop.QuadPart - mytimer->start.QuadPart) / frequency.QuadPart);
#elif defined __MACH__
    static mach_timebase_info_data_t info = { 0, 0 };
    if (info.numer == 0)
        mach_timebase_info(&info);
    long long stop = mach_absolute_time();
    double time = ((stop - mytimer->start) * (double) info.numer / info.denom) / 1.0E9;
#else
    struct timespec s;
    long long stop;
    clock_gettime(CLOCK_MONOTONIC, &s);
    stop = (long long)s.tv_sec * (long long)1.0E6 + (long long)s.tv_nsec / (long long)1.0E3;
    double time = ((double)(stop - mytimer->start) / 1.0E6);
#endif
    return time;
}

/* Random number generator */
double random(double min, double max)
{
    return floor(((double)rand() / ((unsigned int)RAND_MAX + 1)) * (max - min + 1) + min);
}

/* Populate input */
void populateInput(LibreOfficeDeviceEvaluationIO* testData)
{
    srand((unsigned int)time(NULL));
    double* input0 = &testData->input0[0];
    double* input1 = &testData->input1[0];
    double* input2 = &testData->input2[0];
    double* input3 = &testData->input3[0];
    for (unsigned long i = 0; i < testData->inputSize; i++)
    {
        input0[i] = random(0, i);
        input1[i] = random(0, i);
        input2[i] = random(0, i);
        input3[i] = random(0, i);
    }
}
/* Encode score object as byte string */
ds_status serializeScore(ds_device* device, void** serializedScore, unsigned int* serializedScoreSize)
{
    *serializedScoreSize = sizeof(LibreOfficeDeviceScore);
    *serializedScore = (void*)new unsigned char[*serializedScoreSize];
    memcpy(*serializedScore, device->score, *serializedScoreSize);
    return DS_SUCCESS;
}

/* Parses byte string and stores in score object */
ds_status deserializeScore(ds_device* device, const unsigned char* serializedScore, unsigned int serializedScoreSize)
{
    // check that serializedScoreSize == sizeof(LibreOfficeDeviceScore);
    device->score = new LibreOfficeDeviceScore;
    memcpy(device->score, serializedScore, serializedScoreSize);
    return DS_SUCCESS;
}

/* Releases memory held by score */
ds_status releaseScore(void* score)
{
    if (NULL != score)
    {
        delete (LibreOfficeDeviceScore*)score;
    }
    return DS_SUCCESS;
}

/* Evaluate devices */
ds_status evaluateScoreForDevice(ds_device* device, void* evalData)
{
    if (DS_DEVICE_OPENCL_DEVICE == device->type)
    {
        /* Evaluating an OpenCL device */
        LOG_PRINTF("[DS] Device: \"" << device->oclDeviceName << "\" (OpenCL) evaluation...");
        cl_int clStatus;
        /* Check for 64-bit float extensions */
        size_t aDevExtInfoSize = 0;
        clStatus = clGetDeviceInfo(device->oclDeviceID, CL_DEVICE_EXTENSIONS, 0, NULL, &aDevExtInfoSize);
        DS_CHECK_STATUS(clStatus, "evaluateScoreForDevice::clGetDeviceInfo");

        char* aExtInfo = new char[aDevExtInfoSize];
        clStatus = clGetDeviceInfo(device->oclDeviceID, CL_DEVICE_EXTENSIONS, sizeof(char) * aDevExtInfoSize, aExtInfo, NULL);
        DS_CHECK_STATUS(clStatus, "evaluateScoreForDevice::clGetDeviceInfo");
        bool bKhrFp64Flag = false;
        bool bAmdFp64Flag = false;
        const char* buildOption = NULL;
        std::string tmpStr("-Dfp_t=double -Dfp_t4=double4 -Dfp_t16=double16 -DINPUTSIZE=");
        std::ostringstream tmpOStrStr;
        tmpOStrStr << std::dec << INPUTSIZE;
        tmpStr.append(tmpOStrStr.str());

        if ((std::string(aExtInfo)).find("cl_khr_fp64") != std::string::npos)
        {
            bKhrFp64Flag = true;
            //buildOption = "-D KHR_DP_EXTENSION -Dfp_t=double -Dfp_t4=double4 -Dfp_t16=double16";
            tmpStr.append(" -DKHR_DP_EXTENSION");
            buildOption = tmpStr.c_str();
        }
        else if ((std::string(aExtInfo)).find("cl_amd_fp64") != std::string::npos)
        {
            bAmdFp64Flag = true;
            //buildOption = "-D AMD_DP_EXTENSION -Dfp_t=double -Dfp_t4=double4 -Dfp_t16=double16";
            tmpStr.append(" -DAMD_DP_EXTENSION");
            buildOption = tmpStr.c_str();
        }
        delete[] aExtInfo;

        if (!bKhrFp64Flag && !bAmdFp64Flag)
        {
            /* No 64-bit float support */
            device->score = (void*)new LibreOfficeDeviceScore;
            ((LibreOfficeDeviceScore*)device->score)->fTime = DBL_MAX;
            ((LibreOfficeDeviceScore*)device->score)->bNoCLErrors = true;
        }
        else
        {
            /* 64-bit float support present */

            /* Create context and command queue */
            cl_context  clContext = clCreateContext(NULL, 1, &device->oclDeviceID, NULL, NULL, &clStatus);
            DS_CHECK_STATUS(clStatus, "evaluateScoreForDevice::clCreateContext");
            cl_command_queue clQueue = clCreateCommandQueue(clContext, device->oclDeviceID, 0, &clStatus);
            DS_CHECK_STATUS(clStatus, "evaluateScoreForDevice::clCreateCommandQueue");

            /* Build program */
            cl_program clProgram = clCreateProgramWithSource(clContext, 1, &source, sourceSize, &clStatus);
            DS_CHECK_STATUS(clStatus, "evaluateScoreForDevice::clCreateProgramWithSource");
            clStatus = clBuildProgram(clProgram, 1, &device->oclDeviceID, buildOption, NULL, NULL);
            DS_CHECK_STATUS(clStatus, "evaluateScoreForDevice::clBuildProgram");
            if (CL_SUCCESS != clStatus)
            {
                /* Build program failed */
                size_t length;
                char* buildLog;
                clStatus = clGetProgramBuildInfo(clProgram, device->oclDeviceID, CL_PROGRAM_BUILD_LOG, 0, NULL, &length);
                buildLog = (char*)malloc(length);
                clGetProgramBuildInfo(clProgram, device->oclDeviceID, CL_PROGRAM_BUILD_LOG, length, buildLog, &length);
                LOG_PRINTF("[OCL] Build Errors" << std::endl << buildLog);
                free(buildLog);

                device->score = (void*)new LibreOfficeDeviceScore;
                ((LibreOfficeDeviceScore*)device->score)->fTime = DBL_MAX;
                ((LibreOfficeDeviceScore*)device->score)->bNoCLErrors = false;
            }
            else
            {
                /* Build program succeeded */
                timer kernelTime;
                timerStart(&kernelTime);

                /* Run kernel */
                LibreOfficeDeviceEvaluationIO* testData = (LibreOfficeDeviceEvaluationIO*)evalData;
                cl_kernel clKernel = clCreateKernel(clProgram, "DynamicKernel", &clStatus);
                DS_CHECK_STATUS(clStatus, "evaluateScoreForDevice::clCreateKernel");
                cl_mem clResult = clCreateBuffer(clContext, CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR, sizeof(cl_double) * testData->outputSize, &testData->output[0], &clStatus);
                DS_CHECK_STATUS(clStatus, "evaluateScoreForDevice::clCreateBuffer::clResult");
                cl_mem clInput0 = clCreateBuffer(clContext, CL_MEM_READ_ONLY  | CL_MEM_USE_HOST_PTR, sizeof(cl_double) * testData->inputSize,  &testData->input0[0], &clStatus);
                DS_CHECK_STATUS(clStatus, "evaluateScoreForDevice::clCreateBuffer::clInput0");
                cl_mem clInput1 = clCreateBuffer(clContext, CL_MEM_READ_ONLY  | CL_MEM_USE_HOST_PTR, sizeof(cl_double) * testData->inputSize,  &testData->input1[0], &clStatus);
                DS_CHECK_STATUS(clStatus, "evaluateScoreForDevice::clCreateBuffer::clInput1");
                cl_mem clInput2 = clCreateBuffer(clContext, CL_MEM_READ_ONLY  | CL_MEM_USE_HOST_PTR, sizeof(cl_double) * testData->inputSize,  &testData->input2[0], &clStatus);
                DS_CHECK_STATUS(clStatus, "evaluateScoreForDevice::clCreateBuffer::clInput2");
                cl_mem clInput3 = clCreateBuffer(clContext, CL_MEM_READ_ONLY  | CL_MEM_USE_HOST_PTR, sizeof(cl_double) * testData->inputSize,  &testData->input3[0], &clStatus);
                DS_CHECK_STATUS(clStatus, "evaluateScoreForDevice::clCreateBuffer::clInput3");
                clStatus = clSetKernelArg(clKernel, 0, sizeof(cl_mem), (void*)&clResult);
                DS_CHECK_STATUS(clStatus, "evaluateScoreForDevice::clSetKernelArg::clResult");
                clStatus = clSetKernelArg(clKernel, 1, sizeof(cl_mem), (void*)&clInput0);
                DS_CHECK_STATUS(clStatus, "evaluateScoreForDevice::clSetKernelArg::clInput0");
                clStatus = clSetKernelArg(clKernel, 2, sizeof(cl_mem), (void*)&clInput1);
                DS_CHECK_STATUS(clStatus, "evaluateScoreForDevice::clSetKernelArg::clInput1");
                clStatus = clSetKernelArg(clKernel, 3, sizeof(cl_mem), (void*)&clInput2);
                DS_CHECK_STATUS(clStatus, "evaluateScoreForDevice::clSetKernelArg::clInput2");
                clStatus = clSetKernelArg(clKernel, 4, sizeof(cl_mem), (void*)&clInput3);
                DS_CHECK_STATUS(clStatus, "evaluateScoreForDevice::clSetKernelArg::clInput3");
                size_t globalWS[1] = { testData->outputSize };
                size_t localSize[1] = { 64 };
                clStatus = clEnqueueNDRangeKernel(clQueue, clKernel, 1, 0, globalWS, localSize, 0, NULL, NULL);
                DS_CHECK_STATUS(clStatus, "evaluateScoreForDevice::clEnqueueNDRangeKernel");
                clFinish(clQueue);
                clReleaseMemObject(clInput3);
                clReleaseMemObject(clInput2);
                clReleaseMemObject(clInput1);
                clReleaseMemObject(clInput0);
                clReleaseMemObject(clResult);
                clReleaseKernel(clKernel);

                device->score = (void*)new LibreOfficeDeviceScore;
                ((LibreOfficeDeviceScore*)device->score)->fTime = timerCurrent(&kernelTime);
                ((LibreOfficeDeviceScore*)device->score)->bNoCLErrors = true;
            }

            clReleaseProgram(clProgram);
            clReleaseCommandQueue(clQueue);
            clReleaseContext(clContext);
        }
    }
    else
    {
        /* Evaluating an Native CPU device */
        LOG_PRINTF("[DS] Device: \"CPU\" (Native) evaluation...");
        timer kernelTime;
        timerStart(&kernelTime);

        LibreOfficeDeviceEvaluationIO* testData = (LibreOfficeDeviceEvaluationIO*)evalData;
        for (unsigned long j = 0; j < testData->outputSize; j++)
        {
            double fAverage = 0.0f;
            double fMin = DBL_MAX;
            double fSoP = 0.0f;
            for (unsigned long i = 0; i < testData->inputSize; i++)
            {
                fAverage += testData->input0[i];
                fMin = ((fMin < testData->input1[i]) ? fMin : testData->input1[i]);
                fSoP += testData->input2[i] * testData->input3[i];
            }
            fAverage /= testData->inputSize;
            testData->output[j] = fAverage + (fMin * fSoP);
        }

        // InterpretTail - the S/W fallback is nothing like as efficient
        // as any good openCL implementation: no SIMD, tons of branching
        // in the inner loops etc. Generously characterise it as only 10x
        // slower than the above.
        float fInterpretTailFactor = 10.0;

        device->score = (void*)new LibreOfficeDeviceScore;
        ((LibreOfficeDeviceScore*)device->score)->fTime = timerCurrent(&kernelTime);
        ((LibreOfficeDeviceScore*)device->score)->bNoCLErrors = true;

        ((LibreOfficeDeviceScore*)device->score)->fTime *= fInterpretTailFactor;
    }
    return DS_SUCCESS;
}

/* Pick best device */
ds_status pickBestDevice(ds_profile* profile, int* bestDeviceIdx)
{
    double bestScore = DBL_MAX;
    *bestDeviceIdx = -1;

    for (unsigned int d = 0; d < profile->numDevices; d++)
    {
        ds_device device = profile->devices[d];
        LibreOfficeDeviceScore *pScore = (LibreOfficeDeviceScore*)device.score;

        double fScore = DBL_MAX;
        if (pScore)
        {
            fScore = pScore->fTime;
        }
        else
        {
            LOG_PRINTF("Unusual null score");
        }

        if (DS_DEVICE_OPENCL_DEVICE == device.type)
        {
            LOG_PRINTF("[DS] Device[" << d << "] " << device.oclDeviceName << " (OpenCL) score is " << fScore);
        }
        else
        {
            LOG_PRINTF("[DS] Device[" << d << "] CPU (Native) score is " << fScore);
        }
        if (fScore < bestScore)
        {
            bestScore = fScore;
            *bestDeviceIdx = d;
        }
    }
    if (DS_DEVICE_OPENCL_DEVICE == profile->devices[*bestDeviceIdx].type)
    {
        LOG_PRINTF("[DS] Selected Device[" << *bestDeviceIdx << "]: " << profile->devices[*bestDeviceIdx].oclDeviceName << "(OpenCL).");
    }
    else
    {
        LOG_PRINTF("[DS] Selected Device[" << *bestDeviceIdx << "]: CPU (Native).");
    }

    return DS_SUCCESS;
}

/* Return device ID for matching device name */
int matchDevice(ds_profile* profile, char* deviceName)
{
    int deviceMatch = -1;
    for (unsigned int d = 0; d < profile->numDevices - 1; d++)
    {
        if ((std::string(profile->devices[d].oclDeviceName)).find(deviceName) != std::string::npos) deviceMatch = d;
    }
    if (std::string("NATIVE_CPU").find(deviceName) != std::string::npos) deviceMatch = profile->numDevices - 1;
    return deviceMatch;
}

/*************************************************************************/
/* EXTERNAL FUNCTIONS                                                    */
/*************************************************************************/
ds_device getDeviceSelection(const char* sProfilePath, bool bForceSelection)
{
    /* Run only if device is not yet selected */
    if (!bIsDeviceSelected || bForceSelection)
    {
        /* Setup */
        ds_status status;
        ds_profile* profile = NULL;
        status = initDSProfile(&profile, "LibreOffice v0.1");

        if (!profile)
        {
            // failed to initialize profile.
            selectedDevice.type = DS_DEVICE_NATIVE_CPU;
            return selectedDevice;
        }

        /* Try reading scores from file */
        std::string tmpStr(sProfilePath);
        const char* fileName = tmpStr.append("sc_opencl_device_profile.dat").c_str();
        if (!bForceSelection)
        {
            status = readProfileFromFile(profile, deserializeScore, fileName);
        }
        else
        {
            status = DS_INVALID_PROFILE;
            LOG_PRINTF("[DS] Performing forced profiling.");
        }
        if (DS_SUCCESS != status)
        {
            if (!bForceSelection)
            {
                LOG_PRINTF("[DS] Profile file not available (" << fileName << "); performing profiling.");
            }

            /* Populate input data for micro-benchmark */
            boost::scoped_ptr<LibreOfficeDeviceEvaluationIO> testData(new LibreOfficeDeviceEvaluationIO);
            testData->inputSize  = INPUTSIZE;
            testData->outputSize = OUTPUTSIZE;
            testData->input0.resize(testData->inputSize);
            testData->input1.resize(testData->inputSize);
            testData->input2.resize(testData->inputSize);
            testData->input3.resize(testData->inputSize);
            testData->output.resize(testData->outputSize);
            populateInput(testData.get());

            /* Perform evaluations */
            unsigned int numUpdates;
            status = profileDevices(profile, DS_EVALUATE_ALL, evaluateScoreForDevice, (void*)testData.get(), &numUpdates);

            if (DS_SUCCESS == status)
            {
                /* Write scores to file */
                status = writeProfileToFile(profile, serializeScore, fileName);
                if (DS_SUCCESS == status)
                {
                    LOG_PRINTF("[DS] Scores written to file (" << fileName << ").");
                }
                else
                {
                    LOG_PRINTF("[DS] Error saving scores to file (" << fileName << "); scores not written to file.");
                }
            }
            else
            {
                LOG_PRINTF("[DS] Unable to evaluate performance; scores not written to file.");
            }
        }
        else
        {
            LOG_PRINTF("[DS] Profile read from file (" << fileName << ").");
        }

        /* Pick best device */
        int bestDeviceIdx;
        pickBestDevice(profile, &bestDeviceIdx);

        /* Overide if necessary */
        char* overrideDeviceStr = getenv("SC_OPENCL_DEVICE_OVERRIDE");
        if (NULL != overrideDeviceStr)
        {
            int overrideDeviceIdx = matchDevice(profile, overrideDeviceStr);
            if (-1 != overrideDeviceIdx)
            {
                LOG_PRINTF("[DS] Overriding Device Selection (SC_OPENCL_DEVICE_OVERRIDE=" << overrideDeviceStr << ").");
                bestDeviceIdx = overrideDeviceIdx;
                if (DS_DEVICE_OPENCL_DEVICE == profile->devices[bestDeviceIdx].type)
                {
                    LOG_PRINTF("[DS] Selected Device[" << bestDeviceIdx << "]: " << profile->devices[bestDeviceIdx].oclDeviceName << " (OpenCL).");
                }
                else
                {
                    LOG_PRINTF("[DS] Selected Device[" << bestDeviceIdx << "]: CPU (Native).");
                }
            }
            else
            {
                LOG_PRINTF("[DS] Ignoring invalid SC_OPENCL_DEVICE_OVERRIDE=" << overrideDeviceStr << ").");
            }
        }

        /* Final device selection */
        selectedDevice = profile->devices[bestDeviceIdx];
        bIsDeviceSelected = true;

        /* Release profile */
        releaseDSProfile(profile, releaseScore);
    }
    return selectedDevice;
}

}}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
