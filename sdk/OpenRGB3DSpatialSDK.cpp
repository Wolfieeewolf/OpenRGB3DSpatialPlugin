#include <QCoreApplication>
#include <QVariant>
#include "OpenRGB3DSpatialSDK.h"

extern "C" const ORGB3DGridAPI* OpenRGB3DSpatial_GetAPI()
{
    QVariant v = qApp->property("OpenRGB3DSpatialGridAPI");
    if(!v.isValid()) return nullptr;
#ifdef _WIN32
    qulonglong val = v.value<qulonglong>();
#else
    qulonglong val = v.value<qulonglong>();
#endif
    return reinterpret_cast<const ORGB3DGridAPI*>((uintptr_t)val);
}

extern "C" void OpenRGB3DSpatial_SetAPI(const ORGB3DGridAPI* api)
{
    qApp->setProperty("OpenRGB3DSpatialGridAPI", QVariant::fromValue<qulonglong>((qulonglong)(uintptr_t)api));
}

