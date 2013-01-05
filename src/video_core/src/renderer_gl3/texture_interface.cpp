/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    texture_interface.cpp
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2012-12-05
 * @brief   Texture manager interface for the GL3 renderer
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

#include "texture_interface.h"
#include "utils.h"

TextureInterface::TextureInterface(const RendererGL3* parent) {
    parent_ = parent;
}

TextureInterface::~TextureInterface() {
}

/**
 * Create a new texture in the backend renderer
 * @param active_texture_unit Active texture unit to bind to for creation
 * @param cache_entry CacheEntry to create texture for
 * @param raw_data Raw texture data
 * @return a pointer to CacheEntry::BackendData with renderer-specific texture data
 */
TextureManager::CacheEntry::BackendData* TextureInterface::Create(int active_texture_unit, 
    const TextureManager::CacheEntry& cache_entry, u8* raw_data, bool efb_copy, u32 efb_copy_addr) {

    BackendData* backend_data = new BackendData();

    glActiveTexture(GL_TEXTURE0 + active_texture_unit);

    if (!efb_copy) {
        glGenTextures(1, &backend_data->handle_);
        
        glBindTexture(GL_TEXTURE_2D, backend_data->handle_);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cache_entry.width_, cache_entry.height_, 0, GL_RGBA,
            GL_UNSIGNED_BYTE, raw_data);
    } else {
        RendererGL3::GLFramebufferObject* efb_copy_fbo = 
            parent_->efb_copy_cache_->FetchFromHash(efb_copy_addr);

        _ASSERT_MSG(TGP, efb_copy_fbo != NULL, "EFB copy FBO never was initialized!");

        backend_data->handle_ = efb_copy_fbo->texture;

        glBindTexture(GL_TEXTURE_2D, backend_data->handle_);
    }
    return backend_data;
}

/**
 * Delete a texture from the backend renderer
 * @param backend_data Renderer-specific texture data used by renderer to remove it
 */
void TextureInterface::Delete(TextureManager::CacheEntry::BackendData* backend_data) {
    glDeleteTextures(1, &(static_cast<BackendData*>(backend_data)->handle_));
    delete backend_data;
}

/**
 * Binds a texture to the backend renderer
 * @param active_texture_unit Active texture unit to bind to
 * @param backend_data Pointer to renderer-specific data used for binding
 */
void TextureInterface::Bind(int active_texture_unit, 
    const TextureManager::CacheEntry::BackendData* backend_data) {
    glActiveTexture(GL_TEXTURE0 + active_texture_unit);
    glBindTexture(GL_TEXTURE_2D, static_cast<const BackendData*>(backend_data)->handle_);
}

/**
 * Updates the texture parameters
 * @param active_texture_unit Active texture unit to update the parameters for
 * @param tex_mode_0 BP TexMode0 register to use for the update
 * @param tex_mode_1 BP TexMode1 register to use for the update
 */
void TextureInterface::UpdateParameters(int active_texture_unit, const gp::BPTexMode0& tex_mode_0,
    const gp::BPTexMode1& tex_mode_1) {
    static const GLint gl_tex_wrap[4] = {
        GL_CLAMP_TO_EDGE,
        GL_REPEAT,
        GL_MIRRORED_REPEAT,
        GL_REPEAT
    };
    static const GLint gl_mag_filter[2] = {
        GL_NEAREST,
        GL_LINEAR
    };
    static const GLint gl_min_filter[8] = {
        GL_NEAREST,
        GL_NEAREST_MIPMAP_NEAREST,
        GL_NEAREST_MIPMAP_LINEAR,
        GL_NEAREST,
        GL_LINEAR,
        GL_LINEAR_MIPMAP_NEAREST,
        GL_LINEAR_MIPMAP_LINEAR,
        GL_LINEAR
    };
    glActiveTexture(GL_TEXTURE0 + active_texture_unit);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_mag_filter[tex_mode_0.mag_filter]);
    /* TODO(ShizZy): Replace this code. Works sortof for autogenerated mip maps, but it's deprecated
            OpenGL. Currently, forward compatability is enabled, so anything deprecated will not work.
    if (tex_mode_0.use_mipmaps()) {
        glTexParameterf(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 
            gl_min_filter[tex_mode_0.min_filter & 7]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, tex_mode_1.min_lod >> 4);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, tex_mode_1.max_lod >> 4);
        glTexEnvf(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, (tex_mode_0.lod_bias / 31.0f));
    } else {*/
        glTexParameterf(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 
            (tex_mode_0.min_filter >= 4) ? GL_LINEAR : GL_NEAREST);
    //}
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl_tex_wrap[tex_mode_0.wrap_s]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl_tex_wrap[tex_mode_0.wrap_t]);

    //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 2);
}
