#include "Graph.hpp"
#include <tuttle/host/ofx/OfxhClipImage.hpp>
#include <tuttle/host/graph/GraphExporter.hpp>
#include <iostream>
#include <sstream>

namespace tuttle {
namespace host {
namespace core {

Graph::Graph()
{
}

Graph::Graph( const Graph& other )
: _graph(other._graph)
, _nodes(other._nodes)
, _instanceCount(other._instanceCount)
{
}

Graph::~Graph()
{
}

Graph::Node& Graph::createNode( const std::string& id ) throw( exception::LogicError )
{
	ofx::imageEffect::ImageEffectPlugin* plug = Core::instance().getImageEffectPluginById( id );

	if( !plug )
		throw exception::LogicError( "Plugin not found. plug ("+id+")" );

	plug->loadAndDescribeActions(); ///< @todo tuttle keep here or move inside getImageEffectPluginById

	ofx::imageEffect::Instance* plugInst = NULL;
	if( plug->supportsContext( kOfxImageEffectContextFilter ) )
	{
		plugInst = plug->createInstance( kOfxImageEffectContextFilter, NULL );
	}
	else if( plug->supportsContext( kOfxImageEffectContextGenerator ) )
	{
		plugInst = plug->createInstance( kOfxImageEffectContextGenerator, NULL );
	}
	else if( plug->supportsContext( kOfxImageEffectContextGeneral ) )
	{
		plugInst = plug->createInstance( kOfxImageEffectContextGeneral, NULL );
	}
	else
	{
		throw exception::LogicError( "Plugin contexts not supported by the host. ("+id+")" );
	}


	if( !plugInst )
		throw exception::LogicError( "Plugin not found. plugInst ("+id+")" );
	EffectInstance* node = dynamic_cast<EffectInstance*>( plugInst );
	if( !node )
		throw exception::LogicError( "Plugin not found ("+id+")." );

	std::stringstream uniqueName;
	uniqueName << node->getLabel() << ++_instanceCount[node->getLabel()];
	node->setName( uniqueName.str() );

	_nodes.insert( node->getName(), node );
	addToGraph( *node );

	return *node;
}

void Graph::addToGraph( EffectInstance& node )
{
	graph::Vertex v( node.getName(), node );
	_nodesDescriptor[node.getName()] = _graph.addVertex( v );
}

void Graph::removeFromGraph( EffectInstance& node ) throw( exception::LogicError )
{
	//	graph::Vertex v( node.getName(), &node );
	//
	//	_nodesList.find( &node );
	//	_nodes[node.getName()] = node;
	//	_nodesDescriptor[node.getName()] = _graph.addVertex( v );
}

void Graph::deleteNode( const EffectInstance& node ) throw( exception::LogicError )
{

}

void Graph::connect( const Node& out, const Node& in ) throw( exception::LogicError )
{
	const ofx::attribute::ClipImageInstanceSet::ClipImageVector& inClips = in.getClipsByOrder();
	const ofx::attribute::ClipImageInstanceSet::ClipImageMap& inClipsMap = in.getClips();

	const ofx::attribute::AttributeInstance* inAttr;

	if( inClips.size() == 1 )
	{
		inAttr = &inClips[0];
	}
	else if( inClips.size() > 1 )
	{
		const ofx::attribute::ClipImageInstanceSet::ClipImageMap::const_iterator it = inClipsMap.find( kOfxSimpleSourceAttributeName );
		if( it != inClipsMap.end() )
		{
			inAttr = it->second;
		}
		else
		{
			// search "Source"
			inAttr = &inClips[0];
		}
	}
	else // if( inClips.empty() )
	{
		throw exception::LogicError( "Connection failed : no clip." );
	}
	connect( out, in, *inAttr );
}


void Graph::connect( const Node& out, const Node& in, const ofx::attribute::AttributeInstance& inAttr ) throw( exception::LogicError )
{
	graph::Edge e( out.getName(), in.getName(), inAttr.getName() );
	_graph.addEdge( _nodesDescriptor[out.getName()], _nodesDescriptor[in.getName()], e );
}

void Graph::unconnectNode( const EffectInstance& node ) throw( exception::LogicError )
{

}

void Graph::dumpToStdOut()
{
	std::cout
	<< "graph dump" << std::endl
	<< "\tnode count: " << "-" << std::endl;
	_graph.dumpToStdOut();

	graph::GraphExporter<graph::Vertex, graph::Edge>::exportAsDOT( _graph, "testDOTExport.dot" );
}

void Graph::compute()
{
	_graph.test_dfs();
}

}
}
}
