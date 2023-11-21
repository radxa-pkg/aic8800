# device property config

PRODUCT_PROPERTY_OVERRIDES += \
   ro.boot.hardware=$(TARGET_BOARD_PLATFORM)

# pureAndroid disable screen lock
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    ro.lockscreen.disable.default=true

#Enable tv_source for cut memory.
ifeq ($(ENABLE_TV_SOURCE),true)
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    ro.config.tv.source=true
endif

# Adb service port
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    service.adb.tcp.port=5555

#set default timezone
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    persist.sys.timezone=Asia/Shanghai \

# When set to TRUE this flag sets EGL_SWAP_BEHAVIOR_PRESERVED_BIT in eglSwapBuffers which will end up
#   preserving the whole frame causing a significant increase in memory bandwidth and decrease in performance
PRODUCT_PROPERTY_OVERRIDES += \
    debug.hwui.render_dirty_regions=false

# hwui changed skiagl to opengl
PRODUCT_PROPERTY_OVERRIDES += \
    debug.hwui.renderer=opengl

#hisilicon system property
PRODUCT_PROPERTY_OVERRIDES += \
   ro.prop.debug.tv=false  \
   ro.config.media_vol_steps=100

PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
   persist.app.support=all

#hippo board select
PRODUCT_PROPERTY_OVERRIDES += \
   ro.build.board.select=0

#oad update property
PRODUCT_PROPERTY_OVERRIDES += \
   prop.oad.property=oui_3:hwmode_1:hwversion_1:swmode_1:swversion_2

ifneq (,$(filter full tif,$(PRODUCT_SUPPORT_ANDROIDTV_TYPE)))
#cec device logicAddress
PRODUCT_PROPERTY_OVERRIDES += \
   ro.hdmi.device_type=0 \
   ro.vendor.hisi.tif_mode=cec

PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    ro.config.hisi.tif_mode=cec
endif

#-----------------------------------------------------------------------
#media property
#-----------------------------------------------------------------------
#hiplayer set switch black as default
PRODUCT_PROPERTY_OVERRIDES += \
    media.hp.switch.black=true \
    media.hp.ff.hiplayer=true

#mediacodec output mode
PRODUCT_PROPERTY_OVERRIDES += \
    media.mc.output=overlay

ifeq ($(strip $(HISILICON_TEE)),true)
PRODUCT_PROPERTY_OVERRIDES += \
    ro.media.hp.drm.tee=true
else
PRODUCT_PROPERTY_OVERRIDES += \
    ro.media.hp.drm.tee=false
endif

#-----------------------------------------------------------------------
#graphic property
#-----------------------------------------------------------------------
PRODUCT_PROPERTY_OVERRIDES += \
    persist.hisi.hwcursor=true \
    ro.hisi.hwcursor.fb=/dev/graphics/fb3 \
    ro.opengles.version=131072

PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    persist.sys.display.4k2k=0

# disalbe surfaceflinger backpressure flag In order to
# # avoid HWC2.0 maybe not signaled release fence yet.
PRODUCT_PROPERTY_OVERRIDES += \
    debug.sf.disable_backpressure=1

#-----------------------------------------------------------------------
# Optimized Boot
#-----------------------------------------------------------------------
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    persist.sys.boot.optimize=true

#-----------------------------------------------------------------------
# starting up business property
#-----------------------------------------------------------------------
PRODUCT_PROPERTY_OVERRIDES += \
    ro.prop.bootanim.path=/atv/bootvideo/bootanimation.zip \
    service.bootvideo.volume=-1 \
    prop.atv.init=false \
    prop.dtv.init=false \
    prop.service.bootop.type=bootanim \
    persist.service.strbootop.type=none \
    persist.prop.dfx.enable=true \
    persist.prop.xbug.enable=false

#-----------------------------------------------------------------------
# tee propety
#-----------------------------------------------------------------------

# add drm service enable property for PlayReady/Widevine
PRODUCT_PROPERTY_OVERRIDES += drm.service.enabled=true

# widevine debug switch
PRODUCT_PROPERTY_OVERRIDES += drm.widevine.debug=false

#-----------------------------------------------------------------------
# lowram property
# 1024: 1024M (default)
# 512:  512M
# undefine: disable
#-----------------------------------------------------------------------
ifdef PRODUCT_SUPPORT_LOWRAM_LEVEL

    LOCAL_CACHE_LIMIT_COUNT := 4
    ifeq ($(PRODUCT_SUPPORT_LOWRAM_LEVEL),512)

        # set fb size for 720p
        PRODUCT_PROPERTY_OVERRIDES += \
            persist.display.720p=true

        # critical, 512M
        PRODUCT_PROPERTY_OVERRIDES += \
            ro.lr.ram_level=critical \
            ro.lr.process_ram_limit_level=critical

        # disable camera
        PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
            config.disable_cameraservice=true

        LOCAL_CACHE_LIMIT_COUNT := 2
    else
        # low, 1024M
        PRODUCT_PROPERTY_OVERRIDES += \
            ro.lr.ram_level=low \
            ro.lr.process_ram_limit_level=low

        # set fb size for 1080p
        PRODUCT_PROPERTY_OVERRIDES += \
            persist.display.720p=false
    endif

    # Use property to set extra_free, instead of dynamic-adjustment
    PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
        sys.sysctl.extra_free_kbytes=636

    # Limit cached-process's count to a maximum, kill the oldest one if too many.
    # set -1 to disable this limitation
    PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
        persist.sys.lr.cache_limit_count=$(LOCAL_CACHE_LIMIT_COUNT)

    # Enable/Disable HiRamManager in HiRMService
    PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
        persist.sys.lr.ram_manager_enable=true

    # Enable/Disable zRam
    PRODUCT_PROPERTY_OVERRIDES += \
        ro.lr.zram_enable=true

    # Set fault_around_bytes, default value is 65536
    PRODUCT_PROPERTY_OVERRIDES += \
        ro.lr.fault_around=4096

    # Enable/Disable jit
    PRODUCT_PROPERTY_OVERRIDES += \
        dalvik.vm.usejit=true
else
    # normal, 1536M or above
    PRODUCT_PROPERTY_OVERRIDES += \
        ro.lr.ram_level=normal

    # set density to 240 by default
    PRODUCT_PROPERTY_OVERRIDES += \
        ro.sf.lcd_density=240
endif

#-----------------------------------------------------------------------
# key property
#-----------------------------------------------------------------------
ifeq ($(strip $(PRODUCT_SUPPORT_LOW_BAND)),true)
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    ro.config.key_repeat_delay=100
else
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    ro.config.key_repeat_delay=50
endif

#-----------------------------------------------------------------------
# storage property
#-----------------------------------------------------------------------
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    persist.sys.sdcardfs=force_on \
    persist.sys.adoptable=force_on

#-----------------------------------------------------------------------
# wifi property
#-----------------------------------------------------------------------
PRODUCT_PROPERTY_OVERRIDES += \
    wifi.interface=wlan0 \
    wifi.direct.interface=p2p-dev-wlan0 \
    persist.wifi.checkinternet=false

#wifi disguise, default false
PRODUCT_PROPERTY_OVERRIDES += \
    persist.ethernet.wifidisguise=false

#-----------------------------------------------------------------------
# cts property
#-----------------------------------------------------------------------
PRODUCT_PROPERTY_OVERRIDES += \
    ro.radio.noril=true

#-----------------------------------------------------------------------
# network property
#-----------------------------------------------------------------------
#dhcp lease, store dhcp ip address
PRODUCT_PROPERTY_OVERRIDES += \
    persist.network.dhcp.clientip=0.0.0.0

#dhcp lease, store dhcp prefix
PRODUCT_PROPERTY_OVERRIDES += \
    persist.network.dhcp.prefix=0

#check full dhcp, default false
PRODUCT_PROPERTY_OVERRIDES += \
    persist.network.dhcp.full=false

# ethernet switch status: eth0_on eth0_down
PRODUCT_PROPERTY_OVERRIDES += \
    persist.ethernet.status=eth0_on

#network coexist mode,default false
PRODUCT_PROPERTY_OVERRIDES += \
    persist.network.nousefwmark=true

#network coexist mode,default true
PRODUCT_PROPERTY_OVERRIDES += \
    persist.network.coexist=true

#network priority ,default ethernet
#support value : wifi and ethernet
PRODUCT_PROPERTY_OVERRIDES += \
    persist.network.firstpriority=ethernet

ifdef PRODUCT_SUPPORT_LOWRAM_LEVEL
TARGET_SYSTEM_PROP := $(LOCAL_PATH)/system.lowram.prop
else
TARGET_SYSTEM_PROP := $(LOCAL_PATH)/system.prop
endif

#add for androidp healthd fake battery data
PRODUCT_PROPERTY_OVERRIDES += \
    ro.boot.fake_battery=1

PRODUCT_PROPERTY_OVERRIDES += \
    ro.mediaScanner.enable=false

PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    persist.sys.force_rotation_compatible=true

#HiVideoPlayer not support multi video.
PRODUCT_PROPERTY_OVERRIDES += \
    ro.prop.mulvideo.disable=true

#surfaceflinger not crop vo frame
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    persist.grahp.cropframe=true

# rotation, default landscape
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    persist.prop.screenorientation=landscape

#prop for sw playready
PRODUCT_PROPERTY_OVERRIDES += \
    ro.sw.playready.support=true

# suspend prop shutdown/str
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    persist.sys.suspend.mode=str

#vsync message first
PRODUCT_PROPERTY_OVERRIDES += \
    persist.sys.vsyncfirst.enable=false

#Setting country china and language chinese
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    persist.sys.country=CN   \
    persist.sys.language=zh   \
    persist.sys.timezone=Asia/Shanghai


PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
   persist.sys.keystone.enable=true \
   persist.hisi.keystone.enable=true \
   persist.hisi.keystone.lb=0,0 \
   persist.hisi.keystone.lt=0,1080 \
   persist.hisi.keystone.rb=1920,0 \
   persist.hisi.keystone.rt=1920,1080 \
   persist.sys.keystone.lb=0,0 \
   persist.sys.keystone.lt=0,1080 \
   persist.sys.keystone.rb=1920,0 \
   persist.sys.keystone.rt=1920,1080 \
   persist.sys.keystone.update=true \
   persist.sys.keystone.alias.scale=1 \
   persist.sys.keystone.alias.edge=255 \
   persist.hisi.keystone.alias.scale=1 \
   persist.hisi.keystone.alias.edge=255
