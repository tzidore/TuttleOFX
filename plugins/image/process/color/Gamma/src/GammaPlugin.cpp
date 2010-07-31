#include "GammaPlugin.hpp"
#include "GammaProcess.hpp"
#include "GammaDefinitions.hpp"

#include <tuttle/common/utils/global.hpp>
#include <ofxsImageEffect.h>
#include <ofxsMultiThread.h>
#include <boost/gil/gil_all.hpp>

namespace tuttle {
namespace plugin {
namespace gamma {

GammaPlugin::GammaPlugin( OfxImageEffectHandle handle ) :
ImageEffect( handle )
{
    _srcClip = fetchClip( kOfxImageEffectSimpleSourceClipName );
    _dstClip = fetchClip( kOfxImageEffectOutputClipName );
	_gammaType = fetchChoiceParam( kGammaType );
	_master = fetchDoubleParam( kMasterValue );
	_red = fetchDoubleParam( kRedValue );
	_green = fetchDoubleParam( kGreenValue );
	_blue = fetchDoubleParam( kBlueValue );
	_alpha = fetchDoubleParam( kAlphaValue );
	_invert = fetchBooleanParam( kInvert );

	// For updating the UI
	OFX::InstanceChangedArgs instChangedArgs;
	instChangedArgs.reason = OFX::eChangePluginEdit;
	instChangedArgs.renderScale.x = instChangedArgs.renderScale.y = 1.0;
	instChangedArgs.time = 0;
	changedParam( instChangedArgs, kGammaType );
}

GammaProcessParams<GammaPlugin::Scalar> GammaPlugin::getProcessParams( const OfxPointD& renderScale ) const
{
	GammaProcessParams<Scalar> params;
	EGammaType	type;
	double		master,
				red, green, blue,
				alpha;
	bool		invert;
	double		gamma, iGamma,
				rGamma, gGamma, bGamma,
				aGamma;

	type = static_cast<EGammaType>( _gammaType->getValue() );
	master = _master->getValue();
	red = _red->getValue();
	green = _green->getValue();
	blue = _blue->getValue();
	alpha = _alpha->getValue();
	invert = _invert->getValue();
	switch ( type )
	{
		case eGammaTypeGlobal:
			gamma = master;
			if ( invert == true )
			{
				gamma = 1.0 / gamma;
			}
			iGamma = ( gamma == 1.0 ? 1.0 : 1.0 / gamma );
			params.iRGamma =
			params.iGGamma =
			params.iBGamma = iGamma;
			params.iAGamma = 1.0;
			break;
		case eGammaTypeChannels:
			rGamma = red;
			gGamma = green;
			bGamma = blue;
			aGamma = alpha;
			if ( invert == true )
			{
				rGamma = 1.0 / rGamma;
				gGamma = 1.0 / gGamma;
				bGamma = 1.0 / bGamma;
				aGamma = 1.0 / aGamma;
			}
			params.iRGamma = ( rGamma == 1.0 ? 1.0 : 1.0 / rGamma );
			params.iGGamma = ( gGamma == 1.0 ? 1.0 : 1.0 / gGamma );
			params.iBGamma = ( bGamma == 1.0 ? 1.0 : 1.0 / bGamma );
			params.iAGamma = ( aGamma == 1.0 ? 1.0 : 1.0 / aGamma );
			break;
	}
	return params;
}

/**
 * @brief The overridden render function
 * @param[in]   args     Rendering parameters
 */
void GammaPlugin::render( const OFX::RenderArguments &args )
{
	using namespace boost::gil;
    // instantiate the render code based on the pixel depth of the dst clip
    OFX::BitDepthEnum dstBitDepth = _dstClip->getPixelDepth( );
    OFX::PixelComponentEnum dstComponents = _dstClip->getPixelComponents( );

    // do the rendering
    if( dstComponents == OFX::ePixelComponentRGBA )
    {
        switch( dstBitDepth )
        {
            case OFX::eBitDepthUByte :
            {
                GammaProcess<rgba8_view_t> p( *this );
                p.setupAndProcess( args );
                break;
            }
            case OFX::eBitDepthUShort :
            {
                GammaProcess<rgba16_view_t> p( *this );
                p.setupAndProcess( args );
                break;
            }
            case OFX::eBitDepthFloat :
            {
                GammaProcess<rgba32f_view_t> p( *this );
                p.setupAndProcess( args );
                break;
            }
			default:
			{
				COUT_ERROR( "Bit depth (" << mapBitDepthEnumToStr(dstBitDepth) << ") not recognized by the plugin." );
				break;
			}
        }
    }
    else if( dstComponents == OFX::ePixelComponentAlpha )
    {
        switch( dstBitDepth )
        {
            case OFX::eBitDepthUByte :
            {
                GammaProcess<gray8_view_t> p( *this );
                p.setupAndProcess( args );
                break;
            }
            case OFX::eBitDepthUShort :
            {
                GammaProcess<gray16_view_t> p( *this );
                p.setupAndProcess( args );
                break;
            }
            case OFX::eBitDepthFloat :
            {
                GammaProcess<gray32f_view_t> p( *this );
                p.setupAndProcess( args );
                break;
            }
			default:
			{
				COUT_ERROR( "Bit depth (" << mapBitDepthEnumToStr(dstBitDepth) << ") not recognized by the plugin." );
				break;
			}
        }
    }
	else
	{
		COUT_ERROR( "Pixel components (" << mapPixelComponentEnumToStr(dstComponents) << ") not supported by the plugin." );
	}
}

void GammaPlugin::changedParam( const OFX::InstanceChangedArgs &args, const std::string &paramName )
{
	if ( paramName == kGammaType )
	{
		bool bMaster, bRGBA;
		switch ( getGammaType() )
		{
			case eGammaTypeGlobal:
				bMaster = false;
				bRGBA = true;
				break;
			case eGammaTypeChannels:
				bMaster = true;
				bRGBA = false;
				break;
			default: // Error
				bMaster = true;
				bRGBA = true;
				break;
		}
		_master->setIsSecret( bMaster );
		_red->setIsSecret( bRGBA );
		_green->setIsSecret( bRGBA );
		_blue->setIsSecret( bRGBA );
		_alpha->setIsSecret( bRGBA );
//		_master->setEnabled( bMaster );
//		_red->setEnabled( bRGBA );
//		_green->setEnabled( bRGBA );
//		_blue->setEnabled( bRGBA );
//		_alpha->setEnabled( bRGBA );

	}
}

}
}
}