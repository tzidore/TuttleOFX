#include "LibAVVideoReader.hpp"

#include <tuttle/plugin/global.hpp>

#include <boost/cstdint.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/filesystem.hpp>

#include <limits>

namespace tuttle {
namespace plugin {
namespace av {

namespace fs = boost::filesystem;

LibAVVideoReader::LibAVVideoReader()
	: _avFormatOptions( NULL )
	, _stream ( NULL )
	, _format( NULL )
	, _avFrame( NULL )
	, _videoCodec( NULL )
	, _sws_context( NULL )
	, _fpsNum( 0 )
	, _fpsDen( 0 )
	, _currVideoIdx( -1 )
	, _nbFrames( 0 )
	, _width( 0 )
	, _height( 0 )
	, _aspect( 1.0 )
	, _offsetTime( true )
	, _lastSearchPos( -1 )
	, _lastDecodedPos( -1 )
	, _lastDecodedFrame( -1 )
	, _isOpen( false )
{
//	for( int i = 0; i < AVMEDIA_TYPE_NB; ++i )
//	{
//		_avctxOptions[i] = avcodec_alloc_context3( avcodec_find_encoder( i ) );
//	}
	_avFormatOptions = avformat_alloc_context();
	_avFrame         = avcodec_alloc_frame();

}

LibAVVideoReader::~LibAVVideoReader()
{
	close();

	av_free( _avFrame );
//	for( int i = 0; i < AVMEDIA_TYPE_NB; ++i )
//	{
//		av_free( _avctxOptions[i] );
//	}
	av_free( _avFormatOptions );

}

bool LibAVVideoReader::open( const std::string& filename )
{
	if( isOpen() )
	{
		close();
	}

	_isOpen = 0;

#if LIBAVCODEC_VERSION_MAJOR <= 52
	int error = av_open_input_file( &_avFormatOptions, filename.c_str(), _format, 0, 0 );
#else
	int error = avformat_open_input( &_avFormatOptions, filename.c_str(), NULL, NULL );
#endif
	if( error < 0 )
	{
		std::cerr << "avReader: " << libavError_toString( error ) << std::endl;
		_isOpen = 0;
		return false;
	}
	// FIXME_GC: needs to know if it's streamable.
	error = avformat_find_stream_info( _avFormatOptions, NULL );
	if( error < 0 )
	{
		std::cerr << "avReader: " << libavError_toString( error ) << std::endl;
		return false;
	}

	if( !setupStreamInfo() )
	{
		std::cerr << "avReader: Unable to find codec." << std::endl;
		return false;
	}

	_stream = getVideoStream();
	
	AVCodecContext* codecContext = _stream->codec;
	
	if( getVideoStream()->sample_aspect_ratio.num )
	{
		_aspect = av_q2d( getVideoStream()->sample_aspect_ratio );
	}
	else if( codecContext->sample_aspect_ratio.num )
	{
		_aspect = av_q2d( codecContext->sample_aspect_ratio );
	}
	_bitRate = codecContext->bit_rate;

	_data.resize( width() * height() * 3 );

	// hack so seeking works from our intended position.
	if( !strcmp( codecContext->codec->name, "mjpeg" ) || !strcmp( codecContext->codec->name, "dvvideo" ) )
	{
		std::cerr << "avReader: [WARNING] codec is using a specific offsetTime" << codecContext->codec->name << std::endl;
		_offsetTime = false;
	}

	/*
	    std::cout << "MetaData::CREATOR: " << _context->author << std::endl;
	    std::cout << "MetaData::COPYRIGHT: " << _context->copyright << std::endl;
	    std::cout << "MetaData::COMMENT: " << _context->comment << std::endl;
	    std::cout << "MetaData::PROJECT: " << _context->album << std::endl;
	    std::cout << "MetaData::FILE_CREATION_TIME: " << double(_context->timestamp ) << std::endl;
	    std::cout << "libav/num_streams: " << _context->nb_streams << std::endl;
	    std::cout << "MetaData::FRAME_RATE: " << fps( ) << std::endl;
	    std::cout << "libav/codec/codecName: " << codecContext->codec->name << std::endl;

	    meta.setData(MetaData::CREATOR, context_->author);
	    meta.setData(MetaData::COPYRIGHT, context_->copyright);
	    meta.setData(MetaData::COMMENT, context_->comment);
	    meta.setData(MetaData::PROJECT, context_->album);
	    meta.setData(MetaData::FILE_CREATION_TIME, double(context_->timestamp));
	    meta.setData("libav/num_streams", context_->nb_streams);
	    meta.setData(MetaData::FRAME_RATE, fps());
	    meta.setData("libav/codec/codecName", codecContext->codec->name);
	 */

	_isOpen = 1;
	return true;
}

void LibAVVideoReader::close()
{
	_isOpen = false;
	closeVideoCodec();
	if( _avFormatOptions )
	{
#if LIBAVCODEC_VERSION_MAJOR <= 52
		av_close_input_file( _avFormatOptions );
#else
		avformat_close_input( &_avFormatOptions );
#endif
		_avFormatOptions = NULL;
	}
}

bool LibAVVideoReader::read( const int frame )
{
	const int frameNumber = frame % _nbFrames;

	if( frameNumber != frame )
	{
		std::cerr << "Read outside the video range (time:" << frame << ", video size:" << _nbFrames << std::endl;
	}
	if( _lastDecodedFrame + 1 != frameNumber )
	{
		seek( 0 );
		seek( frameNumber );
	}

	av_init_packet( &_pkt );

	bool hasPicture = false;
	int error       = 0;
	//	int i = 0;
	while( error >= 0 && !hasPicture )
	{
		error = av_read_frame( _avFormatOptions, &_pkt );
		// on error or end of file
		if( error < 0 && error != AVERROR_EOF )
		{
			return false;
		}

		if( _videoIdx.size() && _currVideoIdx != -1 && _pkt.stream_index == _videoIdx[_currVideoIdx] )
		{
			hasPicture = decodeImage( frameNumber );
		}

		av_free_packet( &_pkt );
	}

	_lastDecodedFrame = frameNumber;

	return true;
}

bool LibAVVideoReader::setupStreamInfo()
{
	_currVideoIdx = -1;
	for( std::size_t i = 0; i < _avFormatOptions->nb_streams; ++i )
	{
		AVCodecContext* codecContext = _avFormatOptions->streams[i]->codec;
		if( codecContext->codec_id == AV_CODEC_ID_NONE )
		{
			std::cerr << "avReader: Can't find decoder codec_id: CODEC_ID_NONE codecType:" << codecType_toString( codecContext->codec_type ) << std::endl;
			continue;
		}
		if( avcodec_find_decoder( codecContext->codec_id ) == NULL )
		{
			std::cerr << "avReader: Can't find decoder codec_id: " << codecContext->codec_id << " codecType:" << codecType_toString( codecContext->codec_type ) << std::endl;
			continue;
		}

		switch( codecContext->codec_type )
		{
			case AVMEDIA_TYPE_VIDEO:
			{
				_videoIdx.push_back( i );
				if( _currVideoIdx < 0 )
				{
					_currVideoIdx = 0;
				}
				_width  = codecContext->width;
				_height = codecContext->height;
				break;
			}
			// ignore all audio streams
			case AVMEDIA_TYPE_AUDIO:
			case AVMEDIA_TYPE_UNKNOWN:
			case AVMEDIA_TYPE_DATA:
			case AVMEDIA_TYPE_SUBTITLE:
			case AVMEDIA_TYPE_ATTACHMENT:
			default:
				break;
		}
	}

	if( !hasVideo() )
	{
		return false;
	}

	AVStream* stream = getVideoStream();
	if( !stream )
	{
		return false;
	}
	if( stream->codec->coded_frame && stream->codec->coded_frame->interlaced_frame )
	{
		if( stream->codec->coded_frame->top_field_first )
		{
			_interlacment = eInterlacmentUpper;
		}
		else
		{
			_interlacment = eInterlacmentLower;
		}
	}
	else
	{
		_interlacment = eInterlacmentNone;
	}

	if( stream->avg_frame_rate.num != 0 && stream->avg_frame_rate.den != 0 )
	{
		TUTTLE_TLOG( TUTTLE_INFO, "fps " << stream->avg_frame_rate.num << " / " << stream->avg_frame_rate.den );
		_fpsNum = stream->avg_frame_rate.num;
		_fpsDen = stream->avg_frame_rate.den;
	}

	openVideoCodec();

	// Set the duration
	if( _avFormatOptions->duration > 0 )
	{
		_nbFrames = boost::numeric_cast<boost::uint64_t>( ( fps() * (double) _avFormatOptions->duration / (double) AV_TIME_BASE ) );
	}
	else
	{
		_nbFrames = 1 << 29;
	}

	// try to calculate the number of frames
	if( !_nbFrames )
	{
		seek( 0 );
		av_init_packet( &_pkt );
		av_read_frame( _avFormatOptions, &_pkt );
		boost::uint64_t firstPts = _pkt.pts;
		boost::uint64_t maxPts   = firstPts;
		seek( 1 << 29 );
		av_init_packet( &_pkt );
		while( stream && av_read_frame( _avFormatOptions, &_pkt ) >= 0 )
		{
			boost::uint64_t currPts = boost::numeric_cast<boost::uint64_t>( av_q2d( getVideoStream()->time_base ) * ( _pkt.pts - firstPts ) * fps() );
			if( currPts > maxPts )
				maxPts = currPts;
		}

		_nbFrames = maxPts;
	}

	// nb_frames will be set if that information is present in the container,
	// otherwise it will be zero.
	if ( stream->nb_frames )
	{
		if ( (int64_t)_nbFrames != stream->nb_frames )
		{
			std::cerr << "Warning: calculated number of frames ("   <<
			_nbFrames << ") does not match container information (" <<
			stream->nb_frames << ")" << std::endl;
		}
	}

	return true;
}

void LibAVVideoReader::openVideoCodec()
{
	AVStream* stream = getVideoStream();

	if( stream )
	{
		AVCodecContext* codecContext = stream->codec;
		_videoCodec = avcodec_find_decoder( codecContext->codec_id );
		if( _videoCodec == NULL || avcodec_open2( codecContext, _videoCodec, NULL ) < 0 )
		{
			_currVideoIdx = -1;
		}
	}
	else
	{
		_currVideoIdx = -1;
	}
}

void LibAVVideoReader::closeVideoCodec()
{
	AVStream* stream = getVideoStream();

	if( stream && stream->codec )
		avcodec_close( stream->codec );
}

boost::int64_t LibAVVideoReader::getTimeStamp( int pos ) const
{
	boost::int64_t timestamp = boost::numeric_cast<boost::int64_t>( ( (double) pos / fps() ) * AV_TIME_BASE );

	if( (int)_avFormatOptions->start_time != AV_NOPTS_VALUE )
		timestamp += _avFormatOptions->start_time;
	return timestamp;
}

bool LibAVVideoReader::seek( const std::size_t pos )
{
	boost::int64_t offset = getTimeStamp( pos );

	if( _offsetTime )
	{
		offset -= AV_TIME_BASE;
		if( offset < _avFormatOptions->start_time )
			offset = 0;
	}

	AVStream* stream = getVideoStream();
	if( !stream )
		return false;

	avcodec_flush_buffers( stream->codec );
	if( av_seek_frame( _avFormatOptions, -1, offset, AVSEEK_FLAG_BACKWARD ) < 0 )
	{
		return false;
	}

	return true;
}

bool LibAVVideoReader::decodeImage( const int frame )
{
	// search for our picture.
	double pts = 0;

	if( _pkt.dts != (int64_t)AV_NOPTS_VALUE )
	{
		AVStream* stream = getVideoStream();
		if( stream )
		{
			pts = av_q2d( stream->time_base ) * _pkt.dts;
		}
	}

	int curPos = int(pts * fps() + 0.5f);
	if( curPos == _lastSearchPos )
		curPos = _lastSearchPos + 1;
	_lastSearchPos = curPos;

	if( _avFormatOptions->start_time != (int64_t)AV_NOPTS_VALUE )
		curPos -= int(_avFormatOptions->start_time * fps() / AV_TIME_BASE);

	int hasPicture   = 0;
	int curSearch    = 0;
	AVStream* stream = getVideoStream();
	if( !stream )
		return false;

	AVCodecContext* codecContext = stream->codec;
	if( curPos >= frame )
	{
		#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 52, 21, 0 )
		avcodec_decode_video2( codecContext, _avFrame, &hasPicture, &_pkt );
		#else
		avcodec_decode_video( codecContext, _avFrame, &hasPicture, _pkt.data, _pkt.size );
		#endif
	}
	else if( _offsetTime )
	{
		#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 52, 21, 0 )
		avcodec_decode_video2( codecContext, _avFrame, &curSearch, &_pkt );
		#else
		avcodec_decode_video( codecContext, _avFrame, &curSearch, _pkt.data, _pkt.size );
		#endif
	}

	if( !hasPicture )
	{
		return false;
	}

	_lastDecodedPos = _lastSearchPos;

	AVPicture output;
	avpicture_fill( &output, &_data[0], PIX_FMT_RGB24, _width, _height );

	// patched to use swscale instead of img_convert:
	PixelFormat in_pixelFormat  = codecContext->pix_fmt; // pixel format source
	PixelFormat out_pixelFormat = PIX_FMT_RGB24; // pixel format dest

	_sws_context = sws_getCachedContext( _sws_context, width(), height(), in_pixelFormat, width(), height(), out_pixelFormat, SWS_BICUBIC, NULL, NULL, NULL );

	if( !_sws_context )
	{
		std::cerr << "avReader: libav-conversion failed (" << in_pixelFormat << "->" << out_pixelFormat << ")" << std::endl;
		return false;
	}
	int error = sws_scale( _sws_context, _avFrame->data, _avFrame->linesize, 0, height(), output.data, output.linesize );
	if( error < 0 )
	{
		std::cerr << "avReader: libav-conversion failed (" << in_pixelFormat << "->" << out_pixelFormat << ")" << std::endl;
		return false;
	}

	// std::cout << "decodeImage " << frame << " OK" << std::endl;
	return true;
}

}
}
}
