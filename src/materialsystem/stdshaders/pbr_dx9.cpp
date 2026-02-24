//==================================================================================================
//
// Physically Based Rendering shader for brushes and models
// WITH SCREEN-SPACE REFLECTIONS
//
//==================================================================================================

#include "BaseVSShader.h"
#include "cpp_shader_constant_register_map.h"
#include "vtf/vtf.h"

#include "pbr_vs30.inc"
#include "pbr_ps30.inc"

const Sampler_t SAMPLER_BASETEXTURE = SHADER_SAMPLER0;
const Sampler_t SAMPLER_NORMAL = SHADER_SAMPLER1;
const Sampler_t SAMPLER_ENVMAP = SHADER_SAMPLER2;
const Sampler_t SAMPLER_LIGHTWARP = SHADER_SAMPLER3;
const Sampler_t SAMPLER_SHADOWDEPTH = SHADER_SAMPLER4;
const Sampler_t SAMPLER_RANDOMROTATION = SHADER_SAMPLER5;
const Sampler_t SAMPLER_FLASHLIGHT = SHADER_SAMPLER6;
const Sampler_t SAMPLER_LIGHTMAP = SHADER_SAMPLER7;
const Sampler_t SAMPLER_MRAO = SHADER_SAMPLER10;
const Sampler_t SAMPLER_EMISSIVE = SHADER_SAMPLER11;
const Sampler_t SAMPLER_SPECULAR = SHADER_SAMPLER12;
const Sampler_t SAMPLER_SSAO = SHADER_SAMPLER13;
const Sampler_t SAMPLER_THICKNESS = SHADER_SAMPLER14;

static ConVar mat_fullbright("mat_fullbright", "0", FCVAR_CHEAT);
static ConVar mat_specular("mat_specular", "1", FCVAR_NONE);
static ConVar mat_pbr_parallaxmap("mat_pbr_parallaxmap", "1");
static ConVar mat_pbr_subsurfacescattering("mat_pbr_subsurfacescattering", "1");
static ConVar mat_pbr_ssr("mat_pbr_ssr", "1", FCVAR_NONE, "Enable screen-space reflections");
static ConVar mat_pbr_ssr_intensity("mat_pbr_ssr_intensity", "1.0", FCVAR_NONE, "SSR intensity multiplier");
static ConVar mat_pbr_ssr_step_count("mat_pbr_ssr_step_count", "8", FCVAR_NONE, "SSR ray march step count");
static ConVar mat_pbr_ssr_roughness_threshold("mat_pbr_ssr_roughness_threshold", "0.6", FCVAR_NONE, "Only apply SSR below this roughness");

struct PBR_Vars_t
{
    PBR_Vars_t()
    {
        memset(this, 0xFF, sizeof(*this));
    }

    int baseTexture;
    int baseColor;
    int normalTexture;
    int bumpMap;
    int envMap;
    int baseTextureFrame;
    int baseTextureTransform;
    int useParallax;
    int parallaxDepth;
    int parallaxCenter;
    int alphaTestReference;
    int flashlightTexture;
    int flashlightTextureFrame;
    int emissionTexture;
    int mraoTexture;
    int useEnvAmbient;
    int specularTexture;
    int lightwarpTexture;
    int metalnessFactor;
    int roughnessFactor;
    int emissiveFactor;
    int specularFactor;
    int aoFactor;
    int ssaoFactor;
    int useSubsurfaceScattering;
    int thicknessTexture;
    int sssColor;
    int sssIntensity;
    int sssPowerScale;
    int useSSR;
    int ssrIntensity;
    int ssrQuality;
    int ssrRoughnessThreshold;
};

BEGIN_VS_SHADER(PBR, "PBR shader with SSR")

BEGIN_SHADER_PARAMS
SHADER_PARAM(ALPHATESTREFERENCE, SHADER_PARAM_TYPE_FLOAT, "0", "");
SHADER_PARAM(ENVMAP, SHADER_PARAM_TYPE_ENVMAP, "", "Set the cubemap for this material.");
SHADER_PARAM(MRAOTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Texture with metalness in R, roughness in G, ambient occlusion in B.");
SHADER_PARAM(EMISSIONTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Emission texture");
SHADER_PARAM(NORMALTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Normal texture (deprecated, use $bumpmap)");
SHADER_PARAM(BUMPMAP, SHADER_PARAM_TYPE_TEXTURE, "", "Normal texture");
SHADER_PARAM(USEENVAMBIENT, SHADER_PARAM_TYPE_BOOL, "0", "Use the cubemaps to compute ambient light.");
SHADER_PARAM(SPECULARTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Specular F0 RGB map");
SHADER_PARAM(LIGHTWARPTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Lightwarp Texture");
SHADER_PARAM(PARALLAX, SHADER_PARAM_TYPE_BOOL, "0", "Use Parallax Occlusion Mapping.");
SHADER_PARAM(PARALLAXDEPTH, SHADER_PARAM_TYPE_FLOAT, "0.0030", "Depth of the Parallax Map");
SHADER_PARAM(PARALLAXCENTER, SHADER_PARAM_TYPE_FLOAT, "0.5", "Center depth of the Parallax Map");
SHADER_PARAM(METALNESSFACTOR, SHADER_PARAM_TYPE_FLOAT, "1.0", "Metalness factor");
SHADER_PARAM(ROUGHNESSFACTOR, SHADER_PARAM_TYPE_FLOAT, "1.0", "Roughness factor");
SHADER_PARAM(EMISSIVEFACTOR, SHADER_PARAM_TYPE_FLOAT, "1.0", "Emissive factor");
SHADER_PARAM(SPECULARFACTOR, SHADER_PARAM_TYPE_FLOAT, "1.0", "Specular factor");
SHADER_PARAM(AOFACTOR, SHADER_PARAM_TYPE_FLOAT, "1.0", "Ambient occlusion factor");
SHADER_PARAM(SSAOFACTOR, SHADER_PARAM_TYPE_FLOAT, "1.0", "Screen space ambient occlusion factor");
SHADER_PARAM(SUBSURFACESCATTERING, SHADER_PARAM_TYPE_BOOL, "0", "Enable subsurface scattering");
SHADER_PARAM(SSSTHICKNESS, SHADER_PARAM_TYPE_TEXTURE, "", "Thickness map for SSS");
SHADER_PARAM(SSSCOLOR, SHADER_PARAM_TYPE_COLOR, "[1 1 1 1]", "Subsurface scattering color");
SHADER_PARAM(SSSINTENSITY, SHADER_PARAM_TYPE_FLOAT, "1.0", "SSS intensity");
SHADER_PARAM(SSSPOWERSCALE, SHADER_PARAM_TYPE_FLOAT, "1.0", "Power scale for SSS");
SHADER_PARAM(ENABLESSR, SHADER_PARAM_TYPE_BOOL, "1", "Enable screen-space reflections");
SHADER_PARAM(SSRINTENSITY, SHADER_PARAM_TYPE_FLOAT, "1.0", "SSR intensity (0.0 to 2.0)");
SHADER_PARAM(SSRQUALITY, SHADER_PARAM_TYPE_FLOAT, "8", "SSR quality/step count (1-16)");
SHADER_PARAM(SSRROUGHNESSTHRESHOLD, SHADER_PARAM_TYPE_FLOAT, "0.6", "Only apply SSR below this roughness (0.0-1.0)");
END_SHADER_PARAMS

void SetupVars(PBR_Vars_t& info)
{
    info.baseTexture = BASETEXTURE;
    info.baseColor = COLOR;
    info.normalTexture = NORMALTEXTURE;
    info.bumpMap = BUMPMAP;
    info.baseTextureFrame = FRAME;
    info.baseTextureTransform = BASETEXTURETRANSFORM;
    info.alphaTestReference = ALPHATESTREFERENCE;
    info.flashlightTexture = FLASHLIGHTTEXTURE;
    info.flashlightTextureFrame = FLASHLIGHTTEXTUREFRAME;
    info.envMap = ENVMAP;
    info.emissionTexture = EMISSIONTEXTURE;
    info.mraoTexture = MRAOTEXTURE;
    info.useEnvAmbient = USEENVAMBIENT;
    info.specularTexture = SPECULARTEXTURE;
    info.lightwarpTexture = LIGHTWARPTEXTURE;
    info.useParallax = PARALLAX;
    info.parallaxDepth = PARALLAXDEPTH;
    info.parallaxCenter = PARALLAXCENTER;
    info.metalnessFactor = METALNESSFACTOR;
    info.roughnessFactor = ROUGHNESSFACTOR;
    info.emissiveFactor = EMISSIVEFACTOR;
    info.specularFactor = SPECULARFACTOR;
    info.aoFactor = AOFACTOR;
    info.ssaoFactor = SSAOFACTOR;
    info.useSubsurfaceScattering = SUBSURFACESCATTERING;
    info.thicknessTexture = SSSTHICKNESS;
    info.sssColor = SSSCOLOR;
    info.sssIntensity = SSSINTENSITY;
    info.sssPowerScale = SSSPOWERSCALE;
    info.useSSR = ENABLESSR;
    info.ssrIntensity = SSRINTENSITY;
    info.ssrQuality = SSRQUALITY;
    info.ssrRoughnessThreshold = SSRROUGHNESSTHRESHOLD;
}

SHADER_INIT_PARAMS()
{
    if (params[NORMALTEXTURE]->IsDefined())
        params[BUMPMAP]->SetStringValue(params[NORMALTEXTURE]->GetStringValue());

    if (!params[BUMPMAP]->IsDefined())
        params[BUMPMAP]->SetStringValue("dev/flat_normal");

    if (!params[MRAOTEXTURE]->IsDefined())
        params[MRAOTEXTURE]->SetStringValue("dev/pbr_mraotexture");

    if (!params[ENVMAP]->IsDefined())
        params[ENVMAP]->SetStringValue("env_cubemap");

    if (g_pHardwareConfig->SupportsBorderColor())
    {
        params[FLASHLIGHTTEXTURE]->SetStringValue("effects/flashlight_border");
    }
    else
    {
        params[FLASHLIGHTTEXTURE]->SetStringValue("effects/flashlight001");
    }

    INIT_FLOAT_PARM(METALNESSFACTOR, 1.0f);
    INIT_FLOAT_PARM(ROUGHNESSFACTOR, 1.0f);
    INIT_FLOAT_PARM(AOFACTOR, 1.0f);
    INIT_FLOAT_PARM(SSAOFACTOR, 1.0f);
    INIT_FLOAT_PARM(SSSINTENSITY, 1.0f);
    INIT_FLOAT_PARM(SSSPOWERSCALE, 1.0f);
    INIT_FLOAT_PARM(SSRINTENSITY, 1.0f);
    INIT_FLOAT_PARM(SSRQUALITY, 8.0f);
    INIT_FLOAT_PARM(SSRROUGHNESSTHRESHOLD, 0.6f);
}

SHADER_FALLBACK
{
    return 0;
}

SHADER_INIT
{
    PBR_Vars_t info;
    SetupVars(info);

    Assert(info.flashlightTexture >= 0);
    LoadTexture(info.flashlightTexture, TEXTUREFLAGS_SRGB);

    Assert(info.bumpMap >= 0);
    LoadBumpMap(info.bumpMap);

    Assert(info.envMap >= 0);
    int envMapFlags = g_pHardwareConfig->GetHDRType() == HDR_TYPE_NONE ? TEXTUREFLAGS_SRGB : 0;
    envMapFlags |= TEXTUREFLAGS_ALL_MIPS;
    LoadCubeMap(info.envMap, envMapFlags);

    if (info.emissionTexture >= 0 && params[EMISSIONTEXTURE]->IsDefined())
        LoadTexture(info.emissionTexture, TEXTUREFLAGS_SRGB);

    Assert(info.mraoTexture >= 0);
    LoadTexture(info.mraoTexture, 0);

    if (params[info.baseTexture]->IsDefined())
    {
        LoadTexture(info.baseTexture, TEXTUREFLAGS_SRGB);
    }

    if (params[info.specularTexture]->IsDefined())
    {
        LoadTexture(info.specularTexture, TEXTUREFLAGS_SRGB);
    }

    if (params[info.lightwarpTexture]->IsDefined())
    {
        LoadTexture(info.lightwarpTexture);
    }

    if (params[info.thicknessTexture]->IsDefined())
    {
        LoadTexture(info.thicknessTexture, 0);
    }

    if (IS_FLAG_SET(MATERIAL_VAR_MODEL))
    {
        SET_FLAGS2(MATERIAL_VAR2_SUPPORTS_HW_SKINNING);
        SET_FLAGS2(MATERIAL_VAR2_DIFFUSE_BUMPMAPPED_MODEL);
        SET_FLAGS2(MATERIAL_VAR2_NEEDS_TANGENT_SPACES);
        SET_FLAGS2(MATERIAL_VAR2_LIGHTING_VERTEX_LIT);
        SET_FLAGS2(MATERIAL_VAR2_NEEDS_BAKED_LIGHTING_SNAPSHOTS);
        SET_FLAGS2(MATERIAL_VAR2_SUPPORTS_FLASHLIGHT);
        SET_FLAGS2(MATERIAL_VAR2_USE_FLASHLIGHT);
    }
    else
    {
        SET_FLAGS2(MATERIAL_VAR2_LIGHTING_LIGHTMAP);
        SET_FLAGS2(MATERIAL_VAR2_LIGHTING_BUMPED_LIGHTMAP);
        SET_FLAGS2(MATERIAL_VAR2_SUPPORTS_FLASHLIGHT);
        SET_FLAGS2(MATERIAL_VAR2_USE_FLASHLIGHT);
    }

    SET_FLAGS2(MATERIAL_VAR2_USE_GBUFFER0);
    SET_FLAGS2(MATERIAL_VAR2_USE_GBUFFER1);
}

SHADER_DRAW
{
    PBR_Vars_t info;
    SetupVars(info);

    bool bHasBaseTexture = (info.baseTexture != -1) && params[info.baseTexture]->IsTexture();
    bool bHasNormalTexture = (info.bumpMap != -1) && params[info.bumpMap]->IsTexture();
    bool bHasMraoTexture = (info.mraoTexture != -1) && params[info.mraoTexture]->IsTexture();
    bool bHasEmissionTexture = (info.emissionTexture != -1) && params[info.emissionTexture]->IsTexture();
    bool bHasEnvTexture = (info.envMap != -1) && params[info.envMap]->IsTexture();
    bool bIsAlphaTested = IS_FLAG_SET(MATERIAL_VAR_ALPHATEST) != 0;
    bool bHasFlashlight = UsingFlashlight(params);
    bool bHasColor = (info.baseColor != -1) && params[info.baseColor]->IsDefined();
    bool bLightMapped = !IS_FLAG_SET(MATERIAL_VAR_MODEL);
    bool bUseEnvAmbient = (info.useEnvAmbient != -1) && (params[info.useEnvAmbient]->GetIntValue() == 1);
    bool bHasSpecularTexture = (info.specularTexture != -1) && params[info.specularTexture]->IsTexture();
    bool bLightwarpTexture = (info.lightwarpTexture != -1) && params[info.lightwarpTexture]->IsTexture();
    bool bHasSSS = (info.thicknessTexture != -1) && params[info.thicknessTexture]->IsTexture() && params[info.useSubsurfaceScattering]->GetIntValue() == 1 && mat_pbr_subsurfacescattering.GetBool();
    bool bHasSSR = (info.useSSR != -1) && (params[info.useSSR]->GetIntValue() == 1) && mat_pbr_ssr.GetBool();

    BlendType_t nBlendType = EvaluateBlendRequirements(info.baseTexture, true);
    bool bFullyOpaque = (nBlendType != BT_BLENDADD) && (nBlendType != BT_BLEND) && !bIsAlphaTested;

    if (IsSnapshotting())
    {
        pShaderShadow->EnableAlphaTest(bIsAlphaTested);

        if (info.alphaTestReference != -1 && params[info.alphaTestReference]->GetFloatValue() > 0.0f)
        {
            pShaderShadow->AlphaFunc(SHADER_ALPHAFUNC_GEQUAL, params[info.alphaTestReference]->GetFloatValue());
        }

        if (bHasFlashlight)
        {
            pShaderShadow->EnableBlending(true);
            pShaderShadow->BlendFunc(SHADER_BLEND_ONE, SHADER_BLEND_ONE);
        }
        else
        {
            SetDefaultBlendingShadowState(info.baseTexture, true);
        }

        int nShadowFilterMode = bHasFlashlight ? g_pHardwareConfig->GetShadowFilterMode() : 0;

        pShaderShadow->EnableTexture(SAMPLER_BASETEXTURE, true);
        pShaderShadow->EnableSRGBRead(SAMPLER_BASETEXTURE, true);
        pShaderShadow->EnableTexture(SAMPLER_EMISSIVE, true);
        pShaderShadow->EnableSRGBRead(SAMPLER_EMISSIVE, true);
        pShaderShadow->EnableTexture(SAMPLER_LIGHTMAP, true);
        pShaderShadow->EnableSRGBRead(SAMPLER_LIGHTMAP, false);
        pShaderShadow->EnableTexture(SAMPLER_MRAO, true);
        pShaderShadow->EnableSRGBRead(SAMPLER_MRAO, false);
        pShaderShadow->EnableTexture(SAMPLER_NORMAL, true);
        pShaderShadow->EnableSRGBRead(SAMPLER_NORMAL, false);
        pShaderShadow->EnableTexture(SAMPLER_SPECULAR, true);
        pShaderShadow->EnableSRGBRead(SAMPLER_SPECULAR, true);
        pShaderShadow->EnableTexture(SAMPLER_SSAO, true);
        pShaderShadow->EnableSRGBRead(SAMPLER_SSAO, true);
        pShaderShadow->EnableTexture(SAMPLER_THICKNESS, true);
        pShaderShadow->EnableSRGBRead(SAMPLER_THICKNESS, false);

        if (bHasFlashlight)
        {
            pShaderShadow->EnableTexture(SAMPLER_SHADOWDEPTH, true);
            pShaderShadow->SetShadowDepthFiltering(SAMPLER_SHADOWDEPTH);
            pShaderShadow->EnableSRGBRead(SAMPLER_SHADOWDEPTH, false);
            pShaderShadow->EnableTexture(SAMPLER_RANDOMROTATION, true);
            pShaderShadow->EnableTexture(SAMPLER_FLASHLIGHT, true);
            pShaderShadow->EnableSRGBRead(SAMPLER_FLASHLIGHT, true);
        }

        if (bHasEnvTexture)
        {
            pShaderShadow->EnableTexture(SAMPLER_ENVMAP, true);
            if (g_pHardwareConfig->GetHDRType() == HDR_TYPE_NONE)
            {
                pShaderShadow->EnableSRGBRead(SAMPLER_ENVMAP, true);
            }
        }

        if (bLightwarpTexture)
        {
            pShaderShadow->EnableTexture(SAMPLER_LIGHTWARP, true);
            pShaderShadow->EnableSRGBRead(SAMPLER_LIGHTWARP, false);
        }

        pShaderShadow->EnableSRGBWrite(true);

        if (IS_FLAG_SET(MATERIAL_VAR_MODEL))
        {
            unsigned int flags = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_COMPRESSED;
            pShaderShadow->VertexShaderVertexFormat(flags, 1, 0, 0);
        }
        else
        {
            unsigned int flags = VERTEX_POSITION | VERTEX_NORMAL;
            pShaderShadow->VertexShaderVertexFormat(flags, 3, 0, 0);
        }

        int useParallax = params[info.useParallax]->GetIntValue();
        if (!mat_pbr_parallaxmap.GetBool())
        {
            useParallax = 0;
        }

        bool bWorldNormal = (ENABLE_FIXED_LIGHTING_OUTPUTNORMAL_AND_DEPTH ==
                          (IS_FLAG2_SET(MATERIAL_VAR2_USE_GBUFFER0) + 2 * IS_FLAG2_SET(MATERIAL_VAR2_USE_GBUFFER1)));

        DECLARE_STATIC_VERTEX_SHADER(pbr_vs30);
        SET_STATIC_VERTEX_SHADER_COMBO(WORLD_NORMAL, bWorldNormal);
        SET_STATIC_VERTEX_SHADER_COMBO(LIGHTMAPPED, bLightMapped);
        SET_STATIC_VERTEX_SHADER(pbr_vs30);

        DECLARE_STATIC_PIXEL_SHADER(pbr_ps30);
        SET_STATIC_PIXEL_SHADER_COMBO(FLASHLIGHT, bHasFlashlight);
        SET_STATIC_PIXEL_SHADER_COMBO(FLASHLIGHTDEPTHFILTERMODE, nShadowFilterMode);
        SET_STATIC_PIXEL_SHADER_COMBO(LIGHTMAPPED, bLightMapped);
        SET_STATIC_PIXEL_SHADER_COMBO(USEENVAMBIENT, bUseEnvAmbient);
        SET_STATIC_PIXEL_SHADER_COMBO(EMISSIVE, bHasEmissionTexture);
        SET_STATIC_PIXEL_SHADER_COMBO(SPECULAR, bHasSpecularTexture);
        SET_STATIC_PIXEL_SHADER_COMBO(PARALLAXOCCLUSION, useParallax);
        SET_STATIC_PIXEL_SHADER_COMBO(WORLD_NORMAL, bWorldNormal);
        SET_STATIC_PIXEL_SHADER_COMBO(LIGHTWARPTEXTURE, bLightwarpTexture);
        SET_STATIC_PIXEL_SHADER_COMBO(SUBSURFACESCATTERING, bHasSSS);
        SET_STATIC_PIXEL_SHADER_COMBO(SCREEN_SPACE_REFLECTIONS, bHasSSR);
        SET_STATIC_PIXEL_SHADER(pbr_ps30);

        if (bHasFlashlight)
            FogToBlack();
        else
            DefaultFog();

        pShaderShadow->EnableAlphaWrites(bFullyOpaque);

        float flLScale = pShaderShadow->GetLightMapScaleFactor();

        PI_BeginCommandBuffer();
        PI_SetPixelShaderAmbientLightCube(PSREG_AMBIENT_CUBE);
        PI_SetPixelShaderLocalLighting(PSREG_LIGHT_INFO_ARRAY);
        PI_SetModulationPixelShaderDynamicState_LinearScale_ScaleInW(PSREG_DIFFUSE_MODULATION, flLScale);
        PI_EndCommandBuffer();
    }
    else
    {
        bool bLightingOnly = mat_fullbright.GetInt() == 2 && !IS_FLAG_SET(MATERIAL_VAR_NO_DEBUG_OVERRIDE);

        if (bHasBaseTexture)
        {
            BindTexture(SAMPLER_BASETEXTURE, info.baseTexture, info.baseTextureFrame);
        }
        else
        {
            pShaderAPI->BindStandardTexture(SAMPLER_BASETEXTURE, TEXTURE_GREY);
        }

        Vector color;
        if (bHasColor)
        {
            params[info.baseColor]->GetVecValue(color.Base(), 3);
        }
        else
        {
            color = Vector(1.f, 1.f, 1.f);
        }
        pShaderAPI->SetPixelShaderConstant(PSREG_SELFILLUMTINT, color.Base());

        if (bHasEnvTexture)
        {
            BindTexture(SAMPLER_ENVMAP, info.envMap, 0);
        }
        else
        {
            pShaderAPI->BindStandardTexture(SAMPLER_ENVMAP, TEXTURE_BLACK);
        }

        if (bHasEmissionTexture)
        {
            BindTexture(SAMPLER_EMISSIVE, info.emissionTexture, 0);
        }
        else
        {
            pShaderAPI->BindStandardTexture(SAMPLER_EMISSIVE, TEXTURE_BLACK);
        }

        if (bHasNormalTexture)
        {
            BindTexture(SAMPLER_NORMAL, info.bumpMap, 0);
        }
        else
        {
            pShaderAPI->BindStandardTexture(SAMPLER_NORMAL, TEXTURE_NORMALMAP_FLAT);
        }

        if (bHasMraoTexture)
        {
            BindTexture(SAMPLER_MRAO, info.mraoTexture, 0);
        }
        else
        {
            pShaderAPI->BindStandardTexture(SAMPLER_MRAO, TEXTURE_WHITE);
        }

        if (bHasSpecularTexture)
        {
            BindTexture(SAMPLER_SPECULAR, info.specularTexture, 0);
        }
        else
        {
            pShaderAPI->BindStandardTexture(SAMPLER_SPECULAR, TEXTURE_BLACK);
        }

        if (bLightwarpTexture)
        {
            BindTexture(SAMPLER_LIGHTWARP, info.lightwarpTexture, 0);
        }

        if (bHasSSS)
        {
            BindTexture(SAMPLER_THICKNESS, info.thicknessTexture, 0);
        }
        else
        {
            pShaderAPI->BindStandardTexture(SAMPLER_THICKNESS, TEXTURE_WHITE);
        }

        LightState_t lightState;
        pShaderAPI->GetDX9LightState(&lightState);

        if (!IS_FLAG_SET(MATERIAL_VAR_MODEL))
        {
            lightState.m_bAmbientLight = false;
            lightState.m_nNumLights = 0;
        }

        FlashlightState_t flashlightState;
        VMatrix flashlightWorldToTexture;
        bool bFlashlightShadows = false;
        if (bHasFlashlight)
        {
            Assert(info.flashlightTexture >= 0 && info.flashlightTextureFrame >= 0);
            Assert(params[info.flashlightTexture]->IsTexture());
            BindTexture(SAMPLER_FLASHLIGHT, info.flashlightTexture, info.flashlightTextureFrame);
            ITexture* pFlashlightDepthTexture;
            flashlightState = pShaderAPI->GetFlashlightStateEx(flashlightWorldToTexture, &pFlashlightDepthTexture);
            bFlashlightShadows = flashlightState.m_bEnableShadows && (pFlashlightDepthTexture != NULL);

            SetFlashLightColorFromState(flashlightState, pShaderAPI, false, PSREG_FLASHLIGHT_COLOR);

            if (pFlashlightDepthTexture && g_pConfig->ShadowDepthTexture() && flashlightState.m_bEnableShadows)
            {
                BindTexture(SAMPLER_SHADOWDEPTH, pFlashlightDepthTexture, 0);
                pShaderAPI->BindStandardTexture(SAMPLER_RANDOMROTATION, TEXTURE_SHADOW_NOISE_2D);
            }
        }

        MaterialFogMode_t fogType = pShaderAPI->GetSceneFogMode();
        int fogIndex = (fogType == MATERIAL_FOG_LINEAR_BELOW_FOG_Z) ? 1 : 0;

        int numBones = pShaderAPI->GetCurrentNumBones();

        bool bWriteDepthToAlpha = false;
        bool bWriteWaterFogToAlpha = false;
        if (bFullyOpaque)
        {
            bWriteDepthToAlpha = pShaderAPI->ShouldWriteDepthToDestAlpha();
            bWriteWaterFogToAlpha = (fogType == MATERIAL_FOG_LINEAR_BELOW_FOG_Z);
            AssertMsg(!(bWriteDepthToAlpha && bWriteWaterFogToAlpha),
                    "Can't write two values to alpha at the same time.");
        }

        float vEyePos_SpecExponent[4];
        pShaderAPI->GetWorldSpaceCameraPosition(vEyePos_SpecExponent);

        int iEnvMapLOD = 6;
        ITexture* envTexture = params[info.envMap]->GetTextureValue();
        if (envTexture)
        {
            int width = envTexture->GetMappingWidth();
            int mips = 0;
            while (width >>= 1)
                ++mips;
            iEnvMapLOD = mips;
        }

        if (iEnvMapLOD > 12)
            iEnvMapLOD = 12;
        if (iEnvMapLOD < 4)
            iEnvMapLOD = 4;

        vEyePos_SpecExponent[3] = iEnvMapLOD;
        pShaderAPI->SetPixelShaderConstant(PSREG_EYEPOS_SPEC_EXPONENT, vEyePos_SpecExponent, 1);

        pShaderAPI->BindStandardTexture(SAMPLER_LIGHTMAP, TEXTURE_LIGHTMAP_BUMPED);

        DECLARE_DYNAMIC_VERTEX_SHADER(pbr_vs30);
        SET_DYNAMIC_VERTEX_SHADER_COMBO(DOWATERFOG, fogIndex);
        SET_DYNAMIC_VERTEX_SHADER_COMBO(SKINNING, numBones > 0);
        SET_DYNAMIC_VERTEX_SHADER_COMBO(COMPRESSED_VERTS, (int)vertexCompression);
        SET_DYNAMIC_VERTEX_SHADER_COMBO(NUM_LIGHTS, lightState.m_nNumLights);
        SET_DYNAMIC_VERTEX_SHADER(pbr_vs30);

        DECLARE_DYNAMIC_PIXEL_SHADER(pbr_ps30);
        SET_DYNAMIC_PIXEL_SHADER_COMBO(NUM_LIGHTS, lightState.m_nNumLights);
        SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITEWATERFOGTODESTALPHA, bWriteWaterFogToAlpha);
        SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITE_DEPTH_TO_DESTALPHA, bWriteDepthToAlpha);
        SET_DYNAMIC_PIXEL_SHADER_COMBO(PIXELFOGTYPE, pShaderAPI->GetPixelFogCombo());
        SET_DYNAMIC_PIXEL_SHADER_COMBO(FLASHLIGHTSHADOWS, bFlashlightShadows);
        SET_DYNAMIC_PIXEL_SHADER_COMBO(UBERLIGHT, flashlightState.m_bUberlight);
        SET_DYNAMIC_PIXEL_SHADER(pbr_ps30);

        SetVertexShaderTextureTransform(VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, info.baseTextureTransform);

        if (bLightingOnly)
        {
            pShaderAPI->BindStandardTexture(SAMPLER_BASETEXTURE, TEXTURE_GREY);
        }

        if (!mat_specular.GetBool())
        {
            pShaderAPI->BindStandardTexture(SAMPLER_ENVMAP, TEXTURE_BLACK);
        }

        pShaderAPI->SetPixelShaderFogParams(PSREG_FOG_PARAMS);

        ITexture* pAOTexture = pShaderAPI->GetTextureRenderingParameter(TEXTURE_RENDERPARM_AMBIENT_OCCLUSION);

        if (pAOTexture)
            BindTexture(SAMPLER_SSAO, pAOTexture);
        else
            pShaderAPI->BindStandardTexture(SAMPLER_SSAO, TEXTURE_WHITE);

        float flSSAOStrength = 1.0f;
        if (bHasFlashlight)
            flSSAOStrength *= flashlightState.m_flAmbientOcclusion;

        float vMRAOFactors[4] =
        {
            GetFloatParam(info.metalnessFactor, params, 1.0f),
            GetFloatParam(info.roughnessFactor, params, 1.0f),
            GetFloatParam(info.aoFactor, params, 1.0f),
            GetFloatParam(info.ssaoFactor, params, 1.0f) * flSSAOStrength
        };
        pShaderAPI->SetPixelShaderConstant(PSREG_MRAO_FACTORS, vMRAOFactors, 1);

        float vExtraFactors[4] =
        {
            GetFloatParam(info.emissiveFactor, params, 1.0f),
            GetFloatParam(info.specularFactor, params, 1.0f),
            GetFloatParam(info.sssIntensity, params, 1.0f),
            GetFloatParam(info.sssPowerScale, params, 1.0f)
        };
        pShaderAPI->SetPixelShaderConstant(PSREG_EXTRA_FACTORS, vExtraFactors, 1);

        Vector sssColor = Vector(1.0f, 1.0f, 1.0f);
        if (info.sssColor != -1 && params[info.sssColor]->IsDefined())
        {
            params[info.sssColor]->GetVecValue(sssColor.Base(), 3);
        }
        float vSSSColor[4] = { sssColor.x, sssColor.y, sssColor.z, 1.0f };
        pShaderAPI->SetPixelShaderConstant(PSREG_CUSTOM_SSS_PARAMS, vSSSColor, 1);

        if (bHasSSR)
        {
            float vSSRParams[4] =
            {
                0.5f,
                0.25f,
                0.1f,
                mat_pbr_ssr_intensity.GetFloat()
            };
            pShaderAPI->SetPixelShaderConstant(50, vSSRParams, 1);

            float vSSRParams2[4] =
            {
                (float)mat_pbr_ssr_step_count.GetInt(),
                0.35f,
                0.5f,
                mat_pbr_ssr_roughness_threshold.GetFloat()
            };
            pShaderAPI->SetPixelShaderConstant(51, vSSRParams2, 1);
        }

        pShaderAPI->SetScreenSizeForVPOS();

        int nLightingPreviewMode = pShaderAPI->GetIntRenderingParameter(INT_RENDERPARM_ENABLE_FIXED_LIGHTING);
        if (nLightingPreviewMode == ENABLE_FIXED_LIGHTING_OUTPUTNORMAL_AND_DEPTH)
        {
            float vEyeDir[4];
            pShaderAPI->GetWorldSpaceCameraDirection(vEyeDir);

            float flFarZ = pShaderAPI->GetFarZ();
            vEyeDir[0] /= flFarZ;
            vEyeDir[1] /= flFarZ;
            vEyeDir[2] /= flFarZ;
            pShaderAPI->SetVertexShaderConstant(VERTEX_SHADER_SHADER_SPECIFIC_CONST_8, vEyeDir);
        }

        if (bHasFlashlight)
        {
            float atten[4], pos[4], tweaks[4];
            SetFlashLightColorFromState(flashlightState, pShaderAPI, false, PSREG_FLASHLIGHT_COLOR);

            BindTexture(SAMPLER_FLASHLIGHT, flashlightState.m_pSpotlightTexture, flashlightState.m_nSpotlightTextureFrame);

            atten[0] = flashlightState.m_fConstantAtten;
            atten[1] = flashlightState.m_fLinearAtten;
            atten[2] = flashlightState.m_fQuadraticAtten;
            atten[3] = flashlightState.m_FarZAtten;
            pShaderAPI->SetPixelShaderConstant(PSREG_FLASHLIGHT_ATTENUATION, atten, 1);

            pos[0] = flashlightState.m_vecLightOrigin[0];
            pos[1] = flashlightState.m_vecLightOrigin[1];
            pos[2] = flashlightState.m_vecLightOrigin[2];
            pShaderAPI->SetPixelShaderConstant(PSREG_FLASHLIGHT_POSITION_RIM_BOOST, pos, 1);

            pShaderAPI->SetPixelShaderConstant(PSREG_FLASHLIGHT_TO_WORLD_TEXTURE, flashlightWorldToTexture.Base(), 4);

            tweaks[0] = ShadowFilterFromState(flashlightState);
            tweaks[1] = ShadowAttenFromState(flashlightState);
            HashShadow2DJitter(flashlightState.m_flShadowJitterSeed, &tweaks[2], &tweaks[3]);
            pShaderAPI->SetPixelShaderConstant(PSREG_ENVMAP_TINT__SHADOW_TWEAKS, tweaks, 1);

            SetupUberlightFromState(pShaderAPI, flashlightState);
        }

        float flParams[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        flParams[0] = GetFloatParam(info.parallaxDepth, params, 3.0f);
        flParams[1] = GetFloatParam(info.parallaxCenter, params, 3.0f);
        pShaderAPI->SetPixelShaderConstant(PSREG_SHADER_CONTROLS, flParams, 1);
    }

    Draw();
}

END_SHADER