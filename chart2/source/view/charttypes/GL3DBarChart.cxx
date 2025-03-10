/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <GL3DBarChart.hxx>

#include <GL/glew.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include "3DChartObjects.hxx"
#include "GL3DRenderer.hxx"
#include <ExplicitCategoriesProvider.hxx>
#include <DataSeriesHelper.hxx>

#include <osl/time.h>
#ifdef WNT
#include <windows.h>
#endif

#define BENCH_MARK_MODE false

using namespace com::sun::star;

namespace chart {

const size_t STEPS = 200;

namespace {

const float TEXT_HEIGHT = 10.0f;
float DEFAULT_CAMERA_HEIGHT = 500.0f;
const sal_uInt32 ID_STEP = 10;

#if 0
const float BAR_SIZE_X = 15.0f;
const float BAR_SIZE_Y = 15.0f;
#else
const float BAR_SIZE_X = 30.0f;
const float BAR_SIZE_Y = 5.0f;
#endif
const float BAR_DISTANCE_X = 5.0f;
const float BAR_DISTANCE_Y = 5.0;

float calculateTextWidth(const OUString& rText)
{
    return rText.getLength() * 10;
}

double findMaxValue(const boost::ptr_vector<VDataSeries>& rDataSeriesContainer)
{
    double nMax = 0.0;
    for (boost::ptr_vector<VDataSeries>::const_iterator itr = rDataSeriesContainer.begin(),
            itrEnd = rDataSeriesContainer.end(); itr != itrEnd; ++itr)
    {
        const VDataSeries& rDataSeries = *itr;
        sal_Int32 nPointCount = rDataSeries.getTotalPointCount();
        for(sal_Int32 nIndex = 0; nIndex < nPointCount; ++nIndex)
        {
            double nVal = rDataSeries.getYValue(nIndex);
            nMax = std::max(nMax, nVal);
        }
    }
    return nMax;
}

}

class RenderThread : public salhelper::Thread
{
public:
    RenderThread(GL3DBarChart* pChart);

protected:

    void renderFrame();
    GL3DBarChart* mpChart;
};

RenderThread::RenderThread(GL3DBarChart* pChart):
    salhelper::Thread("RenderThread"),
    mpChart(pChart)
{
}

void RenderThread::renderFrame()
{
    if(!mpChart->mbValidContext)
        return;

    mpChart->mrWindow.getContext().makeCurrent();
    Size aSize = mpChart->mrWindow.GetSizePixel();
    mpChart->mpRenderer->SetSize(aSize);
    if(mpChart->mbNeedsNewRender)
    {
        mpChart->mpRenderer->ReleaseTextTexture();
        for(boost::ptr_vector<opengl3D::Renderable3DObject>::iterator itr = mpChart->maShapes.begin(),
                itrEnd = mpChart->maShapes.end(); itr != itrEnd; ++itr)
        {
            itr->render();
        }
    }
    else
    {
        mpChart->mpCamera->render();
    }
    mpChart->mpRenderer->ProcessUnrenderedShape(mpChart->mbNeedsNewRender);
    mpChart->mbNeedsNewRender = false;
    mpChart->mrWindow.getContext().swapBuffers();

}

class RenderOneFrameThread : public RenderThread
{
public:
    RenderOneFrameThread(GL3DBarChart* pChart):
        RenderThread(pChart)
    {}

protected:

    virtual void execute() SAL_OVERRIDE;
};

void RenderOneFrameThread::execute()
{
    osl::MutexGuard aGuard(mpChart->maMutex);
    renderFrame();
}

class RenderAnimationThread : public RenderThread
{
public:
    RenderAnimationThread(GL3DBarChart* pChart, const glm::vec3& rStartPos, const glm::vec3& rEndPos,
            const sal_Int32 nSteps = STEPS):
        RenderThread(pChart),
        maStartPos(rStartPos),
        maEndPos(rEndPos),
        mnSteps(nSteps)
    {
    }

protected:

    virtual void execute() SAL_OVERRIDE;

private:
    glm::vec3 maStartPos;
    glm::vec3 maEndPos;
    sal_Int32 mnSteps;

};

void RenderAnimationThread::execute()
{
    osl::MutexGuard aGuard(mpChart->maMutex);
    glm::vec3 aStep = (maEndPos - maStartPos)/(float)mnSteps;
    for(sal_Int32 i = 0; i < mnSteps; ++i)
    {
        mpChart->maCameraPosition += aStep;
        mpChart->mpCamera->setPosition(mpChart->maCameraPosition);
        /*
        mpChart->maCameraDirection += mpChart->maStepDirection;
        mpChart->mpCamera->setDirection(mpChart->maCameraDirection);
        */
        renderFrame();
    }
    mpChart->mpRenderer->ReleaseScreenTextShapes();
}

class RenderBenchMarkThread : public RenderThread
{
public:
    RenderBenchMarkThread(GL3DBarChart * pChart)
        : RenderThread(pChart)
        , mbExecuting(false)
        , mbNeedFlyBack(false)
        , mnStep(0)
        , mnStepsTotal(0)
        , miFrameCount(0)
    {
        osl_getSystemTime(&mafpsRenderStartTime);
        osl_getSystemTime(&mafpsRenderEndTime);
        osl_getSystemTime(&maScreenTextUpdateStartTime);
        osl_getSystemTime(&maScreenTextUpdateEndTime);
        osl_getSystemTime(&maClickFlyBackStartTime);
        osl_getSystemTime(&maClickFlyBackEndTime);
    }
protected:
    virtual void execute() SAL_OVERRIDE;
private:
    void ProcessMouseEvent();
    void MoveCamera();
    void MoveToBar();
    void MoveToDefault();
    void MoveToCorner();
    void ProcessScroll();
    void UpdateScreenText();
    void UpdateFPS();
    int CalcTimeInterval(TimeValue &startTime, TimeValue &endTime);
    void ProcessClickFlyBack();
private:
    glm::vec3 maStartPos;
    glm::vec3 maEndPos;
    bool mbExecuting;
    bool mbNeedFlyBack;
    glm::vec3 maStep;
    glm::vec3 maStepDirection;
    size_t mnStep;
    size_t mnStepsTotal;
    TimeValue mafpsRenderStartTime;
    TimeValue mafpsRenderEndTime;
    TimeValue maScreenTextUpdateStartTime;
    TimeValue maScreenTextUpdateEndTime;
    TimeValue maClickFlyBackStartTime;
    TimeValue maClickFlyBackEndTime;
    int miFrameCount;
    OUString maFPS;
};

void RenderBenchMarkThread::MoveCamera()
{
    if(mnStep < mnStepsTotal)
    {
        ++mnStep;
        mpChart->maCameraPosition += maStep;
        mpChart->mpCamera->setPosition(mpChart->maCameraPosition);
        mpChart->maCameraDirection += maStepDirection;
        mpChart->mpCamera->setDirection(mpChart->maCameraDirection);
    }
    else
    {
        mnStep = 0;
        mbExecuting = false;
        if (mpChart->maRenderEvent == EVENT_CLICK)
        {
            mpChart->mpRenderer->EndClick();
            mbNeedFlyBack = true;
            osl_getSystemTime(&maClickFlyBackStartTime);
            osl_getSystemTime(&maClickFlyBackEndTime);
        }
        else
            mbNeedFlyBack = false;
        mpChart->maRenderEvent = EVENT_NONE;
    }
}

void RenderBenchMarkThread::MoveToDefault()
{
    if ((mpChart->maCameraPosition == mpChart->maDefaultCameraDirection) &&
        (mpChart->maCameraDirection == mpChart->maDefaultCameraDirection))
    {
        mnStep = 0;
        mbExecuting = false;
        mpChart->maRenderEvent = EVENT_NONE;
        return;
    }
    if (!mbExecuting)
    {
        mnStepsTotal = STEPS;
        maStep = (mpChart->maDefaultCameraPosition - mpChart->maCameraPosition)/((float)mnStepsTotal);
        maStepDirection = (mpChart->maDefaultCameraDirection - mpChart->maCameraDirection)/((float)mnStepsTotal);
        mbExecuting = true;
    }
    MoveCamera();
}

void RenderBenchMarkThread::MoveToBar()
{
    if (!mbExecuting)
    {
        mpChart->mpRenderer->SetPickingMode(true);
        mpChart->mpCamera->render();
        mpChart->mpRenderer->ProcessUnrenderedShape(mpChart->mbNeedsNewRender);
        mpChart->mSelectBarId = mpChart->mpRenderer->GetPixelColorFromPoint(mpChart->maClickPos.X(), mpChart->maClickPos.Y());
        mpChart->mpRenderer->SetPickingMode(false);
        std::map<sal_uInt32, const GL3DBarChart::BarInformation>::const_iterator itr = mpChart->maBarMap.find(mpChart->mSelectBarId);
        if(itr == mpChart->maBarMap.end())
        {
            mpChart->maRenderEvent = EVENT_NONE;
            mpChart->maClickCond.set();
            return;
        }
        const GL3DBarChart::BarInformation& rBarInfo = itr->second;
        mnStepsTotal = STEPS;
        glm::vec3 maTargetPosition = rBarInfo.maPos;
        maTargetPosition.z += 240;
        maTargetPosition.x += BAR_SIZE_X / 2.0f;
        maStep = (maTargetPosition - mpChart->maCameraPosition)/((float)mnStepsTotal);
        glm::vec3 maTargetDirection = rBarInfo.maPos;
        maTargetDirection.x += BAR_SIZE_X / 2.0f;
        maTargetDirection.y += BAR_SIZE_Y / 2.0f;
        maStepDirection = (maTargetDirection - mpChart->maCameraDirection)/((float)mnStepsTotal);
        mpChart->maClickCond.set();
        mbExecuting = true;
        mpChart->mpRenderer->StartClick(mpChart->mSelectBarId);
    }
    MoveCamera();
}

void RenderBenchMarkThread::MoveToCorner()
{
    if (!mbExecuting)
    {
        mnStepsTotal = STEPS;
        maStep = (mpChart->getCornerPosition(mpChart->mnCornerId) - mpChart->maCameraPosition) / float(mnStepsTotal);
        maStepDirection = (glm::vec3(mpChart->mnMaxX/2.0f, mpChart->mnMaxY/2.0f, 0) - mpChart->maCameraDirection)/ float(mnStepsTotal);
        mbExecuting = true;
    }
    MoveCamera();
}

void RenderBenchMarkThread::ProcessScroll()
{
    //will add other process later
    mpChart->maRenderEvent = EVENT_NONE;
}

void RenderBenchMarkThread::ProcessClickFlyBack()
{
    if (!mbNeedFlyBack)
        return;
    osl_getSystemTime(&maClickFlyBackEndTime);
    int aDeltaMs = CalcTimeInterval(maClickFlyBackStartTime, maClickFlyBackEndTime);
    if(aDeltaMs >= 10000)
    {
        mpChart->maRenderEvent = EVENT_MOVE_TO_DEFAULT;
    }
}

void RenderBenchMarkThread::ProcessMouseEvent()
{
    ProcessClickFlyBack();
    if (mpChart->maRenderEvent == EVENT_CLICK)
    {
        MoveToBar();
    }
    else if (mpChart->maRenderEvent == EVENT_MOVE_TO_DEFAULT)
    {
        MoveToDefault();
    }
    else if ((mpChart->maRenderEvent == EVENT_DRAG_LEFT) || (mpChart->maRenderEvent == EVENT_DRAG_RIGHT))
    {
        MoveToCorner();
    }
    else if (mpChart->maRenderEvent == EVENT_SCROLL)
    {
        ProcessScroll();
    }
}

int RenderBenchMarkThread::CalcTimeInterval(TimeValue &startTime, TimeValue &endTime)
{
    TimeValue aTime;
    aTime.Seconds = endTime.Seconds - startTime.Seconds - 1;
    aTime.Nanosec = 1000000000 + endTime.Nanosec - startTime.Nanosec;
    aTime.Seconds += aTime.Nanosec / 1000000000;
    aTime.Nanosec %= 1000000000;
    return aTime.Seconds * 1000+aTime.Nanosec / 1000000;
}

void RenderBenchMarkThread::UpdateFPS()
{
    int aDeltaMs = CalcTimeInterval(mafpsRenderStartTime, mafpsRenderEndTime);
    if(aDeltaMs >= 500)
    {
        osl_getSystemTime(&mafpsRenderEndTime);
        aDeltaMs = CalcTimeInterval(mafpsRenderStartTime, mafpsRenderEndTime);
        int iFPS = miFrameCount * 1000 / aDeltaMs;
        maFPS = OUString("Render FPS: ") + OUString::number(iFPS);
        miFrameCount = 0;
        osl_getSystemTime(&mafpsRenderStartTime);
    }
    osl_getSystemTime(&mafpsRenderEndTime);
    //will add the fps render code here later
}

void RenderBenchMarkThread::UpdateScreenText()
{
    int aDeltaMs = CalcTimeInterval(maScreenTextUpdateStartTime, maScreenTextUpdateEndTime);
    if (aDeltaMs >= 20)
    {
        mpChart->mpRenderer->ReleaseScreenTextShapes();
        UpdateFPS();
        osl_getSystemTime(&maScreenTextUpdateStartTime);
    }
    osl_getSystemTime(&maScreenTextUpdateEndTime);
}

void RenderBenchMarkThread::execute()
{
    while (true)
    {
        {
            osl::MutexGuard aGuard(mpChart->maMutex);
            if (mpChart->mbRenderDie)
                break;
            UpdateScreenText();
            ProcessMouseEvent();
            renderFrame();
        }
        #ifdef WNT
            Sleep(1);
        #else
            TimeValue nTV;
            nTV.Seconds = 0;
            nTV.Nanosec = 1000000;
            osl_waitThread(&nTV);
        #endif
        miFrameCount++;
    }
}

GL3DBarChart::GL3DBarChart(
    const css::uno::Reference<css::chart2::XChartType>& xChartType,
    OpenGLWindow& rWindow) :
    mxChartType(xChartType),
    mpRenderer(new opengl3D::OpenGL3DRenderer()),
    mrWindow(rWindow),
    mpCamera(NULL),
    mbValidContext(true),
    mpTextCache(new opengl3D::TextCache()),
    mnMaxX(0),
    mnMaxY(0),
    mnDistance(0.0),
    mnCornerId(0),
    mbNeedsNewRender(true),
    mbCameraInit(false),
    mbRenderDie(false),
    maRenderEvent(EVENT_NONE),
    mSelectBarId(0),
    miScrollRate(0),
    mbScrollFlg(false)
{
    if (BENCH_MARK_MODE)
    {
        char *scrollFrame = getenv("SCROLL_RATE");
        if (scrollFrame)
        {
            miScrollRate = atoi(scrollFrame);
            if (miScrollRate > 0)
            {
                mbScrollFlg = true;
                mpRenderer->SetScroll();
            }
        }
    }
    Size aSize = mrWindow.GetSizePixel();
    mpRenderer->SetSize(aSize);
    mrWindow.setRenderer(this);
    mpRenderer->init();
}

GL3DBarChart::BarInformation::BarInformation(const glm::vec3& rPos, float nVal,
        sal_Int32 nIndex, sal_Int32 nSeriesIndex):
    maPos(rPos),
    mnVal(nVal),
    mnIndex(nIndex),
    mnSeriesIndex(nSeriesIndex)
{
}

GL3DBarChart::~GL3DBarChart()
{
    if (BENCH_MARK_MODE)
    {
        osl::MutexGuard aGuard(maMutex);
        mbRenderDie = true;
    }

    if(mpRenderThread.is())
        mpRenderThread->join();

    if(mbValidContext)
        mrWindow.setRenderer(NULL);
}

void GL3DBarChart::create3DShapes(const boost::ptr_vector<VDataSeries>& rDataSeriesContainer,
        ExplicitCategoriesProvider& rCatProvider)
{
    osl::MutexGuard aGuard(maMutex);
    mpRenderer->ReleaseShapes();
    // Each series of data flows from left to right, and multiple series are
    // stacked vertically along y axis.

    sal_uInt32 nId = 1;
    float nXEnd = 0.0;
    float nYPos = 0.0;

    const Color aSeriesColor[] = {
        COL_RED, COL_GREEN, COL_YELLOW, COL_BROWN, COL_BLUE
    };

    maCategories.clear();
    maSeriesNames.clear();
    maSeriesNames.reserve(rDataSeriesContainer.size());
    maBarMap.clear();
    maShapes.clear();
    maShapes.push_back(new opengl3D::Camera(mpRenderer.get()));
    mpCamera = static_cast<opengl3D::Camera*>(&maShapes.back());

    sal_Int32 nSeriesIndex = 0;
    sal_Int32 nMaxPointCount = 0;
    double nMaxVal = findMaxValue(rDataSeriesContainer)/100;
    for (boost::ptr_vector<VDataSeries>::const_iterator itr = rDataSeriesContainer.begin(),
            itrEnd = rDataSeriesContainer.end(); itr != itrEnd; ++itr)
    {
        nYPos = nSeriesIndex * (BAR_SIZE_Y + BAR_DISTANCE_Y) + BAR_DISTANCE_Y;

        const VDataSeries& rDataSeries = *itr;
        sal_Int32 nPointCount = rDataSeries.getTotalPointCount();
        nMaxPointCount = std::max(nMaxPointCount, nPointCount);

        bool bMappedFillProperty = rDataSeries.hasPropertyMapping("FillColor");

        // Create series name text object.
        OUString aSeriesName =
            DataSeriesHelper::getDataSeriesLabel(
                rDataSeries.getModel(), mxChartType->getRoleOfSequenceForSeriesLabel());

        maSeriesNames.push_back(aSeriesName);

        if(!aSeriesName.isEmpty())
        {
            maShapes.push_back(new opengl3D::Text(mpRenderer.get(),
                        *mpTextCache, aSeriesName, nId));
            nId += ID_STEP;
            opengl3D::Text* p = static_cast<opengl3D::Text*>(&maShapes.back());
            glm::vec3 aTopLeft, aTopRight, aBottomRight;
            aTopRight.x = -BAR_DISTANCE_Y;
            aTopRight.y = nYPos + BAR_DISTANCE_Y;
            aTopLeft.x = calculateTextWidth(aSeriesName) * -1.0 - BAR_DISTANCE_Y;
            aTopLeft.y = nYPos + BAR_DISTANCE_Y;
            aBottomRight = aTopRight;
            aBottomRight.y -= TEXT_HEIGHT;
            p->setPosition(aTopLeft, aTopRight, aBottomRight);
        }

        sal_Int32 nColor = aSeriesColor[nSeriesIndex % SAL_N_ELEMENTS(aSeriesColor)].GetColor();
        for(sal_Int32 nIndex = 0; nIndex < nPointCount; ++nIndex)
        {
            if(bMappedFillProperty)
            {
                double nPropVal = rDataSeries.getValueByProperty(nIndex, "FillColor");
                if(!rtl::math::isNan(nPropVal))
                    nColor = static_cast<sal_uInt32>(nPropVal);
            }

            float nVal = rDataSeries.getYValue(nIndex);
            float nXPos = nIndex * (BAR_SIZE_X + BAR_DISTANCE_X) + BAR_DISTANCE_X;

            glm::mat4 aScaleMatrix = glm::scale(glm::vec3(BAR_SIZE_X, BAR_SIZE_Y, float(nVal/nMaxVal)));
            glm::mat4 aTranslationMatrix = glm::translate(glm::vec3(nXPos, nYPos, 0.0f));
            glm::mat4 aBarPosition = aTranslationMatrix * aScaleMatrix;

            maBarMap.insert(std::pair<sal_uInt32, BarInformation>(nId,
                        BarInformation(glm::vec3(nXPos, nYPos, float(nVal/nMaxVal)),
                            nVal, nIndex, nSeriesIndex)));

            maShapes.push_back(new opengl3D::Bar(mpRenderer.get(), aBarPosition, nColor, nId));
            nId += ID_STEP;
        }

        float nThisXEnd = nPointCount * (BAR_SIZE_X + BAR_DISTANCE_X);
        if (nXEnd < nThisXEnd)
            nXEnd = nThisXEnd;

        ++nSeriesIndex;
    }

    nYPos += BAR_SIZE_Y + BAR_DISTANCE_Y;

    // X axis
    maShapes.push_back(new opengl3D::Line(mpRenderer.get(), nId));
    nId += ID_STEP;
    opengl3D::Line* pAxis = static_cast<opengl3D::Line*>(&maShapes.back());
    glm::vec3 aBegin;
    aBegin.y = nYPos;
    glm::vec3 aEnd = aBegin;
    aEnd.x = BENCH_MARK_MODE ? (mbScrollFlg ? nXEnd - BAR_SIZE_X : nXEnd) : nXEnd;
    pAxis->setPosition(aBegin, aEnd);
    pAxis->setLineColor(COL_BLUE);

    // Y axis
    maShapes.push_back(new opengl3D::Line(mpRenderer.get(), nId));
    nId += ID_STEP;
    pAxis = static_cast<opengl3D::Line*>(&maShapes.back());
    aBegin.x = aBegin.y = 0;
    aEnd = aBegin;
    aEnd.y = nYPos;
    pAxis->setPosition(aBegin, aEnd);
    pAxis->setLineColor(COL_BLUE);

    // Chart background.
    maShapes.push_back(new opengl3D::Rectangle(mpRenderer.get(), nId));
    nId += ID_STEP;
    opengl3D::Rectangle* pRect = static_cast<opengl3D::Rectangle*>(&maShapes.back());
    glm::vec3 aTopLeft;
    glm::vec3 aTopRight = aTopLeft;
    aTopRight.x = BENCH_MARK_MODE ? (mbScrollFlg ? nXEnd - BAR_SIZE_X : nXEnd + 2 * BAR_DISTANCE_X) : (nXEnd + 2 * BAR_DISTANCE_X);
    glm::vec3 aBottomRight = aTopRight;
    aBottomRight.y = nYPos;
    pRect->setPosition(aTopLeft, aTopRight, aBottomRight);
    pRect->setFillColor(COL_BLACK);
    pRect->setLineColor(COL_BLUE);
    if (mbScrollFlg)
        mpRenderer->SetSceneEdge(BAR_DISTANCE_X - 0.001f, aTopRight.x - BAR_DISTANCE_X);
    else
        mpRenderer->SetSceneEdge(-0.001f, aTopRight.x);
    // Create category texts along X-axis at the bottom.
    uno::Sequence<OUString> aCats = rCatProvider.getSimpleCategories();
    for (sal_Int32 i = 0; i < aCats.getLength(); ++i)
    {
        if (BENCH_MARK_MODE && mbScrollFlg && (i + 1 == aCats.getLength()))
            break;
        maCategories.push_back(aCats[i]);
        if(aCats[i].isEmpty())
            continue;

        float nXPos = i * (BAR_SIZE_X + BAR_DISTANCE_X);

        maShapes.push_back(new opengl3D::Text(mpRenderer.get(), *mpTextCache,
                    aCats[i], nId));
        nId += ID_STEP;
        opengl3D::Text* p = static_cast<opengl3D::Text*>(&maShapes.back());
        aTopLeft.x = nXPos + TEXT_HEIGHT + 0.5 * BAR_SIZE_X;
        aTopLeft.y = nYPos + calculateTextWidth(aCats[i]) + 0.5 * BAR_DISTANCE_Y;
        aTopRight = aTopLeft;
        aTopRight.y = nYPos + 0.5* BAR_DISTANCE_Y;
        aBottomRight.x = nXPos;
        aBottomRight.y = nYPos + 0.5 * BAR_DISTANCE_Y;
        p->setPosition(aTopLeft, aTopRight, aBottomRight);

        // create shapes on other side as well

        maShapes.push_back(new opengl3D::Text(mpRenderer.get(), *mpTextCache,
                    aCats[i], nId));
        nId += ID_STEP;
        p = static_cast<opengl3D::Text*>(&maShapes.back());
        aTopLeft.x = nXPos + TEXT_HEIGHT + 0.5 * BAR_SIZE_X;
        aTopLeft.y =  - 0.5 * BAR_DISTANCE_Y;
        aTopRight = aTopLeft;
        aTopRight.y = -calculateTextWidth(aCats[i]) - 0.5* BAR_DISTANCE_Y;
        aBottomRight.x = nXPos;
        aBottomRight.y = -calculateTextWidth(aCats[i]) - 0.5 * BAR_DISTANCE_Y;
        p->setPosition(aTopLeft, aTopRight, aBottomRight);
    }

    mnMaxX = nMaxPointCount * (BAR_SIZE_X + BAR_DISTANCE_X) + 40;
    mnMaxY = nSeriesIndex * (BAR_SIZE_Y + BAR_DISTANCE_Y) + 40;
    if (!mbCameraInit)
    {
        mnDistance = sqrt(mnMaxX * mnMaxX + mnMaxY * mnMaxY + DEFAULT_CAMERA_HEIGHT * DEFAULT_CAMERA_HEIGHT);
        maDefaultCameraDirection = glm::vec3(mnMaxX * 0.4, mnMaxY * 0.35, 0);
        maDefaultCameraPosition = glm::vec3(maDefaultCameraDirection.x, maDefaultCameraDirection.y - mnDistance, DEFAULT_CAMERA_HEIGHT * 2);
        mnCornerId = 0;
        mbCameraInit = true;
        float pi = 3.1415926f;
        float angleX = -pi / 6.5f;
        float angleZ = -pi / 8.0f;
        glm::mat4 maDefaultRotateMatrix = glm::eulerAngleYXZ(0.0f, angleX, angleZ);
        maDefaultCameraPosition = glm::vec3(maDefaultRotateMatrix * glm::vec4(maDefaultCameraPosition, 1.0f));
        maCameraPosition = maDefaultCameraPosition;
        maCameraDirection = maDefaultCameraDirection;
        mpCamera->setPosition(maCameraPosition);
        mpCamera->setDirection(maCameraDirection);
    }
    else
    {
        mpCamera->setPosition(maCameraPosition);
        mpCamera->setDirection(maCameraDirection);
    }
    if (BENCH_MARK_MODE && (!mpRenderThread.is()))
    {
        //if scroll the bars, set the speed and distance first
        if (mbScrollFlg)
        {
            mpRenderer->SetScrollSpeed((float)(BAR_SIZE_X + BAR_DISTANCE_X) / (float)miScrollRate);
            mpRenderer->SetScrollDistance((float)(BAR_SIZE_X + BAR_DISTANCE_X));
        }
        Size aSize = mrWindow.GetSizePixel();
        mrWindow.getContext().setWinSize(aSize);
        mpRenderThread = rtl::Reference<RenderThread>(new RenderBenchMarkThread(this));
        mrWindow.getContext().resetCurrent();
        mpRenderThread->launch();
    }
    mbNeedsNewRender = true;
}

void GL3DBarChart::update()
{
    if (BENCH_MARK_MODE)
        return;
    if(mpRenderThread.is())
        mpRenderThread->join();
    Size aSize = mrWindow.GetSizePixel();
    mrWindow.getContext().setWinSize(aSize);
    mpRenderThread = rtl::Reference<RenderThread>(new RenderOneFrameThread(this));
    mrWindow.getContext().resetCurrent();
    mpRenderThread->launch();

}

namespace {

class PickingModeSetter
{
private:
    opengl3D::OpenGL3DRenderer* mpRenderer;

public:
    PickingModeSetter(opengl3D::OpenGL3DRenderer* pRenderer):
        mpRenderer(pRenderer)
    {
        mpRenderer->SetPickingMode(true);
    }

    ~PickingModeSetter()
    {
        mpRenderer->SetPickingMode(false);
    }
};

}

void GL3DBarChart::moveToDefault()
{
    if(BENCH_MARK_MODE)
    {
        // add correct handling here!!
        if (maRenderEvent != EVENT_NONE)
            return;

        {
            osl::MutexGuard aGuard(maMutex);
            maRenderEvent = EVENT_MOVE_TO_DEFAULT;
        }
        return;
    }

    if(mpRenderThread.is())
        mpRenderThread->join();

    osl::MutexGuard aGuard(maMutex);
    Size aSize = mrWindow.GetSizePixel();
    mrWindow.getContext().setWinSize(aSize);
    mpRenderThread = rtl::Reference<RenderThread>(new RenderAnimationThread(this, maCameraPosition, maDefaultCameraPosition, STEPS));
    mrWindow.getContext().resetCurrent();
    mpRenderThread->launch();

    /*
     * TODO: moggi: add to thread
    glm::vec3 maTargetDirection = maDefaultCameraDirection;
    maStepDirection = (maTargetDirection - maCameraDirection)/((float)mnStepsTotal);
    */
}

void GL3DBarChart::clickedAt(const Point& rPos, sal_uInt16 nButtons)
{
    if (nButtons == MOUSE_RIGHT)
    {
        moveToDefault();
        return;
    }

    if(nButtons != MOUSE_LEFT)
        return;

    if (BENCH_MARK_MODE)
    {
        // add correct handling here !!
        if (maRenderEvent != EVENT_NONE)
            return;

        {
            osl::MutexGuard aGuard(maMutex);
            maClickPos = rPos;
            maRenderEvent = EVENT_CLICK;
            maClickCond.reset();
        }
        maClickCond.wait();
        return;
    }

    sal_uInt32 nId = 5;
    {
        PickingModeSetter aPickingModeSetter(mpRenderer.get());
        update();
        mpRenderThread->join();
        nId = mpRenderer->GetPixelColorFromPoint(rPos.X(), rPos.Y());
    }
    // we need this update here to render one frame without picking mode being set
    update();

    std::map<sal_uInt32, const BarInformation>::const_iterator itr =
        maBarMap.find(nId);

    if(itr == maBarMap.end())
        return;

    const BarInformation& rBarInfo = itr->second;

    maShapes.push_back(new opengl3D::ScreenText(mpRenderer.get(), *mpTextCache,
                OUString("Value: ") + OUString::number(rBarInfo.mnVal), 0));
    opengl3D::ScreenText* pScreenText = static_cast<opengl3D::ScreenText*>(&maShapes.back());
    pScreenText->setPosition(glm::vec2(-0.9f, 0.9f), glm::vec2(-0.6f, 0.8f));
    pScreenText->render();

    glm::vec3 maTargetPosition = rBarInfo.maPos;
    maTargetPosition.z += 240;
    maTargetPosition.y += BAR_SIZE_Y / 2.0f;
    Size aSize = mrWindow.GetSizePixel();
    mrWindow.getContext().setWinSize(aSize);
    mpRenderThread = rtl::Reference<RenderThread>(new RenderAnimationThread(this, maCameraPosition, maTargetPosition, STEPS));
    mrWindow.getContext().resetCurrent();
    mpRenderThread->launch();

    /*
     * TODO: moggi: add to thread
    glm::vec3 maTargetDirection = rBarInfo.maPos;
    maTargetDirection.x += BAR_SIZE_X / 2.0f;
    maTargetDirection.y += BAR_SIZE_Y / 2.0f;

    maStepDirection = (maTargetDirection - maCameraDirection)/((float)mnStepsTotal);
    */

}

void GL3DBarChart::render()
{
    if (BENCH_MARK_MODE)
        return;

    update();
}

void GL3DBarChart::mouseDragMove(const Point& rStartPos, const Point& rEndPos, sal_uInt16 )
{
    long direction = rEndPos.X() - rStartPos.X();
    osl::MutexGuard aGuard(maMutex);
    if (maRenderEvent == EVENT_NONE)
        maRenderEvent = direction > 0 ? EVENT_DRAG_RIGHT : EVENT_DRAG_LEFT;
    if(direction < 0)
    {
        mnCornerId = (mnCornerId + 1) % 4;
        moveToCorner();
    }
    else if(direction > 0)
    {
        mnCornerId = mnCornerId - 1;
        if(mnCornerId < 0)
            mnCornerId = 3;
        moveToCorner();
    }
}

glm::vec3 GL3DBarChart::getCornerPosition(sal_Int8 nId)
{
    float pi = 3.1415926f;
    switch(nId)
    {
        case 0:
        {
            return glm::vec3(mnMaxX / 2 - mnDistance * sin(pi / 4), mnMaxY / 2 - mnDistance * cos(pi / 4), DEFAULT_CAMERA_HEIGHT * 2);
        }
        break;
        case 1:
        {
            return glm::vec3(mnMaxX / 2 + mnDistance * sin(pi / 4), mnMaxY / 2 - mnDistance * cos(pi / 4), DEFAULT_CAMERA_HEIGHT * 2);
        }
        break;
        case 2:
        {
            return glm::vec3(mnMaxX / 2 + mnDistance * sin(pi / 4), mnMaxY / 2 + mnDistance * cos(pi / 4), DEFAULT_CAMERA_HEIGHT * 2);
        }
        break;
        case 3:
        {
            return glm::vec3(mnMaxX / 2 - mnDistance * sin(pi / 4), mnMaxY / 2 + mnDistance * cos(pi / 4), DEFAULT_CAMERA_HEIGHT * 2);
        }
        break;
        default:
            assert(false);
    }
    return maDefaultCameraPosition;
}

void GL3DBarChart::moveToCorner()
{
    if(BENCH_MARK_MODE)
    {
        // add correct handling here!!
        return;
    }

    if(mpRenderThread.is())
        mpRenderThread->join();

    osl::MutexGuard aGuard(maMutex);

    Size aSize = mrWindow.GetSizePixel();
    mrWindow.getContext().setWinSize(aSize);
    mpRenderThread = rtl::Reference<RenderThread>(new RenderAnimationThread(this, maCameraPosition,
                getCornerPosition(mnCornerId), STEPS));
    mrWindow.getContext().resetCurrent();
    mpRenderThread->launch();

    // TODO: moggi: add to thread
    // maStepDirection = (glm::vec3(mnMaxX/2.0f, mnMaxY/2.0f, 0) - maCameraDirection)/ float(mnStepsTotal);
}

void GL3DBarChart::scroll(long nDelta)
{
    {
        osl::MutexGuard aGuard(maMutex);
        glm::vec3 maDir = glm::normalize(maCameraPosition - maCameraDirection);
        maCameraPosition -= (float((nDelta/10)) * maDir);
        mpCamera->setPosition(maCameraPosition);
        if(BENCH_MARK_MODE)
            maRenderEvent = EVENT_SCROLL;
    }

    update();
}

void GL3DBarChart::contextDestroyed()
{
    osl::MutexGuard aGuard(maMutex);
    mbValidContext = false;
}

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
