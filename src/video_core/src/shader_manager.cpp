/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    shader_manager.cpp
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2013-01-28
 * @brief   Managers shaders
 *
 * @section LICENSE
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * Official project repository can be found at:
 * http://code.google.com/p/gekko-gc-emu/
 */

#include "hash.h"
#include "shader_manager.h"

#include "video_core.h"
#include "cp_mem.h"
#include "bp_mem.h"
#include "crc.h"

#define _VSDEF(str, ...)  vsh_->Write("#define _VSDEF_" str, __VA_ARGS__)
#define _FSDEF(str, ...)  fsh_->Write("#define _FSDEF_" str, __VA_ARGS__)

ShaderManager::ShaderManager(const BackendInterface* backend_interface) {
    backend_interface_  = const_cast<BackendInterface*>(backend_interface);
    cache_              = new CacheContainer();
    active_shader_      = new CacheEntry(); // Something that is empty so this isn't NULL
    vsh_                = new ShaderHeader();
    fsh_                = new ShaderHeader();
}

ShaderManager::~ShaderManager() {
    delete cache_;
    delete vsh_;
    delete fsh_;
}

void ShaderManager::UpdateFlag(Flag flag, int enable) {
    if (enable) {
        state_.fields.flags |= flag;
    } else {
        state_.fields.flags &= ~flag;
    }
}

void ShaderManager::UpdateVertexState(gp::VertexState& vertex_state) {
    state_.fields.vertex_state = vertex_state;
}

void ShaderManager::UpdateGenMode(const gp::BPGenMode& gen_mode) {
    state_.fields.num_stages = gen_mode.num_tevstages;
}

void ShaderManager::UpdateAlphaFunc(const gp::BPAlphaFunc& alpha_func) {
    state_.fields.alpha_func._u32 = alpha_func._u32 & 0xFF0000;
}

void ShaderManager::UpdateEFBFormat(gp::BPPixelFormat efb_format) {
    state_.fields.efb_format = efb_format;
}

void ShaderManager::UpdateTevCombiner(int index, const gp::BPTevCombiner& tev_combiner) {
    state_.fields.tev_combiner[index].color._u32 = tev_combiner.color._u32 & 0xC0FFFF;
    state_.fields.tev_combiner[index].alpha._u32 = tev_combiner.alpha._u32 & 0xC0FFF0;
}

void ShaderManager::UpdateTevOrder(int index, const gp::BPTevOrder& tev_order) {
    state_.fields.tev_order[index]._u32 = tev_order._u32 & 0x3FF3FF;
}

void ShaderManager::GenerateVertexHeader() {
    static const char* vertex_color[] = { "RGB565", "RGB8", "RGBX8", "RGBA4", "RGBA6", "RGBA8" };

    vsh_->Reset();

    if (gp::g_cp_regs.vat_reg_a[gp::g_cur_vat].get_pos_dqf_enabled()) _VSDEF("POS_DQF\n");
    if (gp::g_cp_regs.vcd_lo[0].pos_midx_enable) _VSDEF("POS_MIDX\n");
    if (gp::g_cp_regs.vcd_lo[0].tex0_midx_enable) _VSDEF("TEX_0_MIDX\n");
    if (gp::g_cp_regs.vcd_lo[0].tex1_midx_enable) _VSDEF("TEX_1_MIDX\n");
    if (gp::g_cp_regs.vcd_lo[0].tex2_midx_enable) _VSDEF("TEX_2_MIDX\n");
    if (gp::g_cp_regs.vcd_lo[0].tex3_midx_enable) _VSDEF("TEX_3_MIDX\n");
    if (gp::g_cp_regs.vcd_lo[0].tex4_midx_enable) _VSDEF("TEX_4_MIDX\n");
    if (gp::g_cp_regs.vcd_lo[0].tex5_midx_enable) _VSDEF("TEX_5_MIDX\n");
    if (gp::g_cp_regs.vcd_lo[0].tex6_midx_enable) _VSDEF("TEX_6_MIDX\n");
    if (gp::g_cp_regs.vcd_lo[0].tex7_midx_enable) _VSDEF("TEX_7_MIDX\n");

    _VSDEF("COLOR0_%s\n", vertex_color[gp::g_cp_regs.vat_reg_a[gp::g_cur_vat].col0_type]);
    _VSDEF("COLOR1_%s\n", vertex_color[gp::g_cp_regs.vat_reg_a[gp::g_cur_vat].col1_type]);
}
                                                 
void ShaderManager::GenerateFragmentHeader() {   
    //_fs_def_flag(kFlag_DestinationAlpha,         "DESTINATION_ALPHA");
    int reg_index = 0;
    static const char* clamp[] = { "val", "clamp(val, 0.0, 1.0)" };
    static const char* alpha_logic[] = { "&&", "||", "!=", "==" };
    static const char* alpha_compare_0[] = { "(false)", "(val < ref0)", "(val == ref0)", 
        "(val <= ref0)", "(val > ref0)", "(val != ref0)", "(val >= ref0)", "(true)" };
    static const char* alpha_compare_1[] = { "(false)", "(val < ref1)", "(val == ref1)", 
        "(val <= ref1)", "(val > ref1)", "(val != ref1)", "(val >= ref1)", "(true)" };
    static const char* tev_color_input[] = { "prev.rgb",  "prev.aaa", "color0.rgb", "color0.aaa",
        "color1.rgb", "color1.aaa", "color2.rgb", "color2.aaa", "tex.rgb", "tex.aaa", "ras.rgb",
        "ras.aaa", "vec3(1.0f, 1.0f, 1.0f)", "vec3(0.5f, 0.5f, 0.5f)", "konst.rgb", 
        "vec3(0.0f, 0.0f, 0.0f)"  };
    static const char* tev_alpha_input[] = { "prev.a", "color0.a", "color1.a", "color2.a", "tex.a",
        "ras.a", "konst.a", "0.0f" };
    static const char* tev_dest[] = { "prev", "color0", "color1", "color2" };
    static const char* tev_ras[] = { "vtx_color[0]", "vtx_color[1]", "ERROR", "ERROR", "ERROR",
        "vtx_color[0]", "vtx_color[0]", "vec4(0.0f, 0.0f, 0.0f, 0.0f)" };

    fsh_->Reset();

    _FSDEF("NUM_STAGES %d\n", state_.fields.num_stages);
    _FSDEF("ALPHA_COMPARE(val, ref0, ref1) (!(%s %s %s))\n",
        alpha_compare_0[state_.fields.alpha_func.comp0],
        alpha_logic[state_.fields.alpha_func.logic],
        alpha_compare_1[state_.fields.alpha_func.comp1]);

    for (int stage = 0; stage <= gp::g_bp_regs.genmode.num_tevstages; stage++) {
        reg_index = stage / 2;
        _FSDEF("CLAMP_COLOR_%d(val) %s\n", stage, clamp[gp::g_bp_regs.combiner[stage].color.clamp]);
        _FSDEF("CLAMP_ALPHA_%d(val) %s\n", stage, clamp[gp::g_bp_regs.combiner[stage].alpha.clamp]);
        _FSDEF("STAGE_DEST vec4(%s.rgb, %s.a)\n", 
            tev_dest[gp::g_bp_regs.combiner[gp::g_bp_regs.genmode.num_tevstages].color.dest], 
            tev_dest[gp::g_bp_regs.combiner[gp::g_bp_regs.genmode.num_tevstages].alpha.dest]);

        std::string fix_texture_format = "tex";

        // Texture enabled for stage?
        if (gp::g_bp_regs.tevorder[reg_index].get_enable(stage)) {
            int texcoord = state_.fields.tev_order[reg_index].get_texcoord(stage);
            int texmap = state_.fields.tev_order[reg_index].get_texmap(stage);
            
            // Set texture to 0 if texgen is disabled...
            if (!(u32)texcoord < gp::g_bp_regs.genmode.num_texgens) {
                texcoord = 0;
            }
            /*TextureManager::CacheEntry* tex_entry = video_core::g_texture_manager->active_textures_[texmap];
            if (tex_entry != NULL) {
                // EFB copy adjustments stuff
                if (tex_entry->type_ == TextureManager::kSourceType_EFBCopy) {
                    // 
                    if (tex_entry->efb_copy_data_.copy_exec_.intensity_fmt) {
                        fix_texture_format = "vec4(0.257f * tex.r, 0.504f * tex.g, 0.098f * tex.b, tex.a)";
                    }
                }
            }*/
            _FSDEF("TEXTURE_%d texture2D(texture[%d], vtx_texcoord[%d])\n", 
                stage, texmap, texcoord);

        // This will use white color for texture
        } else {
            _FSDEF("TEXTURE_%d vec4(1.0f, 1.0f, 1.0f, 1.0f)\n", stage);
        }
        _FSDEF("COMBINER_COLOR_A_%d %s\n", stage, 
            tev_color_input[state_.fields.tev_combiner[stage].color.sel_a]);
        _FSDEF("COMBINER_COLOR_B_%d %s\n", stage, 
            tev_color_input[state_.fields.tev_combiner[stage].color.sel_b]);
        _FSDEF("COMBINER_COLOR_C_%d %s\n", stage, 
            tev_color_input[state_.fields.tev_combiner[stage].color.sel_c]);
        _FSDEF("COMBINER_COLOR_D_%d %s\n", stage, 
            tev_color_input[state_.fields.tev_combiner[stage].color.sel_d]);
        _FSDEF("COMBINER_COLOR_DEST_%d %s.rgb\n", stage, 
            tev_dest[state_.fields.tev_combiner[stage].color.dest]);

        _FSDEF("COMBINER_ALPHA_A_%d %s\n", stage, 
            tev_alpha_input[state_.fields.tev_combiner[stage].alpha.sel_a]);
        _FSDEF("COMBINER_ALPHA_B_%d %s\n", stage, 
            tev_alpha_input[state_.fields.tev_combiner[stage].alpha.sel_b]);
        _FSDEF("COMBINER_ALPHA_C_%d %s\n", stage, 
            tev_alpha_input[state_.fields.tev_combiner[stage].alpha.sel_c]);
        _FSDEF("COMBINER_ALPHA_D_%d %s\n", stage, 
            tev_alpha_input[state_.fields.tev_combiner[stage].alpha.sel_d]);
        _FSDEF("COMBINER_ALPHA_DEST_%d %s.a\n", stage, 
            tev_dest[state_.fields.tev_combiner[stage].alpha.dest]);
        _FSDEF("RASCOLOR_%d %s\n", stage, 
            tev_ras[state_.fields.tev_order[reg_index].get_colorchan(stage)]);
    }
    // Do destination alpha?
    if (state_.fields.flags & kFlag_DestinationAlpha) {
        _FSDEF("SET_DESTINATION_ALPHA\n");
    }
    // Adjust color for current EFB format
    switch (state_.fields.efb_format) {
    case gp::kPixelFormat_RGB8_Z24:
        _FSDEF("EFB_FORMAT(val) vec4((round(val.rgb * 255.0f) / 255.0f), 1.0f)\n");
        break;
    case gp::kPixelFormat_RGBA6_Z24:
        _FSDEF("EFB_FORMAT(val) FIX_U6(val)\n");
        break;
    case gp::kPixelFormat_RGB565_Z16:
        _FSDEF("EFB_FORMAT(val) vec4(FIX_U5(val.r), FIX_U6(val.g), FIX_U5(val.b), 1.0f)\n");
        break;
    default:
        _FSDEF("EFB_FORMAT(val) vec4(val.rgb, 1.0f)\n");
        break;
    }
}

void ShaderManager::Bind() {
    static CacheEntry   cache_entry;
    cache_entry.hash_ = common::GetHash64(state_.mem, sizeof(State), 0);
    if (cache_entry.hash_ != active_shader_->hash_) {
        active_shader_ = cache_->FetchFromHash(cache_entry.hash_);

        if (NULL == active_shader_) {
            this->GenerateVertexHeader();
            this->GenerateFragmentHeader();

            cache_entry.backend_data_ = backend_interface_->Create(vsh_->Read(), fsh_->Read());

            // Update cache with new information...
            active_shader_ = cache_->Update(cache_entry.hash_, cache_entry);
        }
        backend_interface_->Bind(active_shader_->backend_data_);
    }
    active_shader_->frame_used_ = video_core::g_current_frame;
}