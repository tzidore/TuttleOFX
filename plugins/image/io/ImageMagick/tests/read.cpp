#include <boost/test/unit_test.hpp>

#include <tuttle/host/Graph.hpp>
#include <tuttle/host/InputBufferNode.hpp>

#include <boost/preprocessor/stringize.hpp>

#include <boost/timer.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <iostream>

/**
 * Simple functional test, to read and write an image.
 */

using namespace boost::unit_test;
using namespace tuttle::host;

BOOST_AUTO_TEST_SUITE( plugin_ImageMagick_reader )

BOOST_AUTO_TEST_CASE( process_reader_tif )
{
	TUTTLE_COUT( "******** PROCESS READER IMAGE MAGICK ********" );
	Graph g;

	TUTTLE_COUT( "--> PLUGINS CREATION" );
	Graph::Node& read = g.createNode( "tuttle.imagemagickreader" );

	TUTTLE_COUT( "--> PLUGINS CONFIGURATION" );

	read.getParam( "filename" ).setValue( "TuttleOFX-data/image/tif/Colorful_Night-en.tif" );
	
	TUTTLE_COUT( "--> GRAPH PROCESSING" );
	boost::posix_time::ptime t1a(boost::posix_time::microsec_clock::local_time());
	memory::MemoryCache outputCache;
	g.compute( outputCache, read );
	boost::posix_time::ptime t2a(boost::posix_time::microsec_clock::local_time());

	TUTTLE_COUT( "Process took: " << t2a - t1a );

	std::cout << outputCache << std::endl;

	memory::CACHE_ELEMENT imgRes = outputCache.get( read.getName(), 0 );

	TUTTLE_TCOUT_VAR( imgRes->getROD() );
	BOOST_CHECK_EQUAL( imgRes->getROD().x1, 0 );
	BOOST_CHECK_EQUAL( imgRes->getROD().y1, 0 );
	BOOST_CHECK_NE( imgRes->getROD().x2, 0 );
	BOOST_CHECK_NE( imgRes->getROD().y2, 0 );

	TUTTLE_TCOUT_VAR( imgRes->getBounds() );
	BOOST_CHECK_EQUAL( imgRes->getBounds().x1, 0 );
	BOOST_CHECK_EQUAL( imgRes->getBounds().y1, 0 );
	BOOST_CHECK_NE( imgRes->getBounds().x2, 0 );
	BOOST_CHECK_NE( imgRes->getBounds().y2, 0 );
}

BOOST_AUTO_TEST_CASE( process_nofile )
{
	TUTTLE_COUT( "******** PROCESS READER IMAGE MAGICK NO FILE ********" );
	Graph g;

	TUTTLE_COUT( "--> PLUGINS CREATION" );
	Graph::Node& read = g.createNode( "tuttle.imagemagickreader" );

	TUTTLE_COUT( "--> PLUGINS CONFIGURATION" );
	read.getParam( "filename" ).setValue( "data/no-such-file.dpx" );

	TUTTLE_COUT( "--> GRAPH CONNECTION" );
//	g.connect( read, write );

	TUTTLE_COUT( "---> GRAPH PROCESSING" );
	BOOST_REQUIRE_THROW( g.compute( read ), boost::exception );
}

BOOST_AUTO_TEST_SUITE_END()
