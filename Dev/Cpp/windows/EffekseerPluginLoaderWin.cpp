
#include <memory>
#include <string>
#include <map>
#include <algorithm>

#pragma warning (disable : 4005)
#include "Effekseer.h"

#include "EffekseerRendererGL.h"
#include "EffekseerRendererDX9.h"
#include "EffekseerRendererDX11.h"

#include "../common/IUnityGraphics.h"
#include "../common/EffekseerPluginTexture.h"
#include "../common/EffekseerPluginModel.h"

#include "../opengl/EffekseerPluginLoaderGL.h"

using namespace Effekseer;

namespace EffekseerPlugin
{
	extern UnityGfxRenderer					g_UnityRendererType;
	extern ID3D11Device*					g_D3d11Device;
	extern EffekseerRenderer::Renderer*		g_EffekseerRenderer;

	class TextureLoaderWin : public TextureLoader
	{
		struct TextureResource {
			int referenceCount = 1;
			TextureData texture = {};
		};
		std::map<std::u16string, TextureResource> resources;
	public:
		TextureLoaderWin(
			TextureLoaderLoad load,
			TextureLoaderUnload unload) 
			: TextureLoader(load, unload) {}
		virtual ~TextureLoaderWin() {}
		virtual TextureData* Load( const EFK_CHAR* path, TextureType textureType ){
			// ���\�[�X�e�[�u�����������đ��݂����炻����g��
			auto it = resources.find((const char16_t*)path);
			if (it != resources.end()) {
				it->second.referenceCount++;
				return &it->second.texture;
			}

			// Unity�Ńe�N�X�`�������[�h
			int32_t width, height, format;
			void* texturePtr = load( (const char16_t*)path, &width, &height, &format );
			if (texturePtr == nullptr)
			{
				return nullptr;
			}
			
			// ���\�[�X�e�[�u���ɒǉ�
			auto added = resources.insert( std::make_pair((const char16_t*)path, TextureResource() ) );
			TextureResource& res = added.first->second;
			
			res.texture.Width = width;
			res.texture.Height = height;
			res.texture.TextureFormat = (TextureFormatType)format;

			if (g_UnityRendererType == kUnityGfxRendererD3D11)
			{
				// DX11�̏ꍇ�AUnity�����[�h����̂�ID3D11Texture2D�Ȃ̂ŁA
				// ID3D11ShaderResourceView���쐬����
				HRESULT hr;
				ID3D11Texture2D* textureDX11 = (ID3D11Texture2D*)texturePtr;
				
				D3D11_TEXTURE2D_DESC texDesc;
				textureDX11->GetDesc(&texDesc);
				
				ID3D11ShaderResourceView* srv = nullptr;
				D3D11_SHADER_RESOURCE_VIEW_DESC desc;
				ZeroMemory(&desc, sizeof(desc));
				desc.Format = texDesc.Format;
				desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				desc.Texture2D.MostDetailedMip = 0;
				desc.Texture2D.MipLevels = texDesc.MipLevels;
				hr = g_D3d11Device->CreateShaderResourceView(textureDX11, &desc, &srv);
				if (FAILED(hr))
				{
					return nullptr;
				}

				res.texture.UserPtr = srv;
			} else if (g_UnityRendererType == kUnityGfxRendererD3D9)
			{
				IDirect3DTexture9* textureDX9 = (IDirect3DTexture9*)texturePtr;
				res.texture.UserPtr = textureDX9;
			}

			return &res.texture;
		}
		virtual void Unload( TextureData* source ){
			if (source == nullptr) {
				return;
			}

			// �A�����[�h����e�N�X�`��������
			auto it = std::find_if(resources.begin(), resources.end(), 
				[source](const std::pair<std::u16string, TextureResource>& pair){
					return &pair.second.texture == source;
				});
			if (it == resources.end()) {
				return;
			}

			// �Q�ƃJ�E���^��0�ɂȂ�������ۂɃA�����[�h
			it->second.referenceCount--;
			if (it->second.referenceCount <= 0) {
				if (g_UnityRendererType == kUnityGfxRendererD3D11)
				{
					// �쐬����ID3D11ShaderResourceView���������
					ID3D11ShaderResourceView* srv = (ID3D11ShaderResourceView*)source->UserPtr;
					srv->Release();
				}
				// Unity���ŃA�����[�h
				unload(it->first.c_str());
				resources.erase(it);
			}
		}
	};

	TextureLoader* TextureLoader::Create(
		TextureLoaderLoad load,
		TextureLoaderUnload unload)
	{
		return new TextureLoaderWin( load, unload );
	}

	ModelLoader* ModelLoader::Create(
		ModelLoaderLoad load,
		ModelLoaderUnload unload)
	{
		auto loader = new ModelLoader( load, unload );
		auto internalLoader = g_EffekseerRenderer->CreateModelLoader( loader->GetFileInterface() );
		loader->SetInternalLoader( internalLoader );
		return loader;
	}
};