/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define LOG_NDEBUG              0
#define LOG_TAG                 "standby"
#include <cutils/memory.h>

#include <unistd.h>
#include <utils/Log.h>

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>

#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/ISurfaceComposer.h>
#include <ui/DisplayInfo.h>
#include <ui/Rect.h>
#include <ui/Region.h>
#include <android/native_window.h>
#include <SkGraphics.h>
#include <SkBitmap.h>
#include <SkCanvas.h>
#include <SkDevice.h>
#include <SkStream.h>
#include <SkAndroidCodec.h>

#include <SkCodec.h>




#include <SkColor.h>
#include <SkColorSpace.h>
#include <SkData.h>
#include <SkEncodedImageFormat.h>
#include <SkImage.h>
#include <SkImageEncoder.h>
#include <SkImageGenerator.h>
#include <SkImageInfo.h>
#include <SkPixmap.h>
#include <SkPngChunkReader.h>
#include <SkRect.h>
#include <SkRefCnt.h>
#include <SkSize.h>
#include <SkStream.h>
#include <SkString.h>
#include <SkTypes.h>
#include <SkUnPreMultiply.h>
#include <SkAutoMalloc.h>
#include <SkColorSpacePriv.h>
#include <Resources.h>
#include "../skia/tools/Resources.h"
#include <hardware/hwcomposer_defs.h>
#include <ToolUtils.h>

#include <ui/DisplayConfig.h>
#include <ui/PixelFormat.h>
#include <ui/Rect.h>
#include <ui/Region.h>

#include <vector>

#include <stdint.h>
#include <inttypes.h>
#include <sys/inotify.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>
#include <fcntl.h>
#include <utils/misc.h>
#include <signal.h>
#include <time.h>

#include <cutils/atomic.h>
#include <cutils/properties.h>

#include <utils/Errors.h>
#include <utils/Log.h>
#include <utils/SystemClock.h>

#include <android-base/properties.h>



using namespace android;



static bool decode_image_file(const char *filename, SkBitmap *bitmap,
                              SkColorType colorType = kN32_SkColorType,
                              bool requireUnpremul = false)
{
    sk_sp < SkData > data(SkData::MakeFromFileName(filename));
    std::unique_ptr < SkCodec > codec(SkCodec::MakeFromData(std::move(data)));

    if (!codec)
    {
        SLOGE("error codec");
        return false;
    }

    SkImageInfo     info = codec->getInfo().makeColorType(colorType);

    if (requireUnpremul && kPremul_SkAlphaType == info.alphaType())
    {
        info                = info.makeAlphaType(kUnpremul_SkAlphaType);
    }

    if (!bitmap->tryAllocPixels(info))
    {
        SLOGE("alloc pixels failed");
        return false;
    }

    return SkCodec::kSuccess == codec->getPixels(info, bitmap->getPixels(), bitmap->rowBytes());
}


int main(int argc, char * *argv)
{
    const char      DISPLAYS_PROP_NAME[] = "persist.service.bootanim.displays";

    sp < IBinder > mDisplayToken;
    DisplayConfig   displayConfig;
    SkBitmap        bmp;
    SkRegion        clipReg;
    const char  *data_src_path = "/data/misc/standby.png";
    const char  *vendor_src_path = "/vendor/media/standby.png";
    const char  *src_path = "";
    ANativeWindow_Buffer outBuffer;

    if((access(data_src_path,F_OK))!=-1)
    {
        src_path = data_src_path;
    }else if((access(vendor_src_path,F_OK))!=-1)
    {
        src_path = vendor_src_path;
    }else{
        SLOGE("no standby png exist");
        return - 1;
    }

    if (!decode_image_file(src_path, &bmp))
    {
        SLOGE("drawLogoPic decode_image_file error path:%s", src_path);
        return - 1;
    }
    SLOGI("src_path :%s", src_path);


    int32_t width = bmp.width();
    int32_t height = bmp.height();

    // set up the thread-pool
    sp < ProcessState > proc(ProcessState::self());
    ProcessState::self()->startThreadPool();

    // create a client to surfaceflinger
    sp < SurfaceComposerClient > client = new SurfaceComposerClient();
    mDisplayToken       = SurfaceComposerClient::getInternalDisplayToken();

    //获取屏幕的宽高等信息
    const status_t error = SurfaceComposerClient::getActiveDisplayConfig(mDisplayToken, &displayConfig);

    if (error != NO_ERROR)
        return error;

    ui::Size resolution = displayConfig.resolution;
    SurfaceComposerClient::Transaction t;


    sp < SurfaceControl > surfaceControl = client->createSurface(String8("standby"),
                                           resolution.getWidth(), resolution.getHeight(), PIXEL_FORMAT_RGBA_8888);
    SLOGI("width= %d \n", width);
    SLOGI("height= %d \n", height);
    SLOGI("resolution.getWidth()= %d \n", resolution.getWidth());
    SLOGI("resolution.getHeight()= %d \n", resolution.getHeight());

    if (surfaceControl == NULL || !surfaceControl->isValid())
    {
        SLOGE("Error creating sprite surface.");
        return - 1;
    }


    // this guest property specifies multi-display IDs to show the boot animation
    // multiple ids can be set with comma (,) as separator, for example:
    // setprop persist.boot.animation.displays 19260422155234049,19261083906282754
    Vector < uint64_t > physicalDisplayIds;
    char displayValue[PROPERTY_VALUE_MAX] = "";

    property_get(DISPLAYS_PROP_NAME, displayValue, "");
    bool isValid = displayValue[0] != '\0';

    if (isValid)
    {
        char *p = displayValue;

        while (*p)
        {
            if (!isdigit(*p) && *p != ',')
            {
                isValid             = false;
                break;
            }
            p++;
        }

        if (!isValid)
            SLOGE("Invalid syntax for the value of system prop: %s", DISPLAYS_PROP_NAME);
    }

    if (isValid)
    {
        std::istringstream stream(displayValue);

        for (PhysicalDisplayId id; stream >> id; )
        {
            physicalDisplayIds.add(id);

            if (stream.peek() == ',')
                stream.ignore();
        }

        // In the case of multi-display, boot animation shows on the specified displays
        // in addition to the primary display
        auto ids = SurfaceComposerClient::getPhysicalDisplayIds();
        constexpr uint32_t LAYER_STACK = 0;

        for (auto id : physicalDisplayIds)
        {
            if (std::find(ids.begin(), ids.end(), id) != ids.end())
            {
                sp < IBinder > token = SurfaceComposerClient::getPhysicalDisplayToken(id);

                if (token != nullptr)
                    t.setDisplayLayerStack(token, LAYER_STACK);
            }
        }

        t.setLayerStack(surfaceControl, LAYER_STACK);
    }

    sp < Surface > surface = surfaceControl->getSurface();
    t.setLayer(surfaceControl, 0x40000000).apply();

    //t.setPosition(surfaceControl, 0,0);
    t.apply();
    surface = surfaceControl->getSurface();

    /**********************************************************************************/
    surface->lock(&outBuffer, NULL);

    SkBitmap surfaceBitmap;

    SkImageInfo info = SkImageInfo::Make(resolution.getWidth(), resolution.getHeight(), kN32_SkColorType,
                           kPremul_SkAlphaType);

    ssize_t bpr = outBuffer.stride * bytesPerPixel(outBuffer.format);

    surfaceBitmap.installPixels(info, outBuffer.bits, bpr);

    SkCanvas surfaceCanvas(surfaceBitmap);
    SkPaint paint;

    //paint.setXfermodeMode(SkXfermode::kSrc_Mode);
    surfaceCanvas.drawBitmap(bmp, 0, 0, &paint);

    status_t status = surface->unlockAndPost();
    SLOGI("standby compelete");

    if (status)
    {
        SLOGE("Error %d unlocking and posting sprite surface after drawing.", status);
    }
    IPCThreadState::self()->joinThreadPool();
    IPCThreadState::self()->stopProcess();
    return 0;
}