#ifndef _TUTTLE_PROCESSVISITORS_HPP_
#define _TUTTLE_PROCESSVISITORS_HPP_

#include "ProcessOptions.hpp"

#include <tuttle/host/memory/MemoryCache.hpp>

#include <boost/graph/properties.hpp>
#include <boost/graph/visitors.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/foreach.hpp>

#include <iostream>
#include <vector>

namespace tuttle {
namespace host {
namespace graph {

template<class TGraph>
inline void connectClips( TGraph& graph )
{
	BOOST_FOREACH( typename TGraph::edge_descriptor ed, graph.getEdges() )
	{
		typename TGraph::Edge& edge           = graph.instance( ed );
		typename TGraph::Vertex& vertexSource = graph.sourceInstance( ed );
		typename TGraph::Vertex& vertexDest   = graph.targetInstance( ed );

		if( ! vertexDest.isFake() && ! vertexSource.isFake() )
		{
			INode& sourceNode = *vertexSource.getProcessNode();
			INode& targetNode = *vertexDest.getProcessNode();
			sourceNode.connect( targetNode, sourceNode.getAttribute( edge.getInAttrName() ) );
		}
	}
}

namespace visitor {

/**
 * @brief Create a new version of a graph with nodes deployed over time.
 *
 * Compute kOfxImageEffectActionGetFramesNeeded for each node if kOfxImageEffectPropTemporalClipAccess is true.
 */
template<class TGraph>
class DeployTime : public boost::default_dfs_visitor
{
public:
	typedef typename TGraph::GraphContainer GraphContainer;
	typedef typename TGraph::Vertex Vertex;
	typedef typename TGraph::Edge Edge;
	typedef typename TGraph::edge_descriptor edge_descriptor;

	DeployTime( TGraph& graph, const OfxTime time )
		: _graph( graph )
		, _time( time )
	{}

	template<class VertexDescriptor, class Graph>
	void finish_vertex( VertexDescriptor v, Graph& g )
	{
		Vertex& vertex = _graph.instance( v );

		TCOUT( "[DEPLOY TIME] " << vertex );
		if( vertex.isFake() )
		{
			BOOST_FOREACH( const edge_descriptor& ed, _graph.getOutEdges( v ) )
			{
				Edge& e = _graph.instance( ed );
				e._timesNeeded[_time].insert( _time );
//				TCOUT( "--- insert edge: " << _time );
			}
			vertex._times.insert( _time );
			return;
		}

		// merge times nedded for all out edges
		BOOST_FOREACH( const edge_descriptor& ed, _graph.getInEdges( v ) )
		{
			const Edge& edge = _graph.instance( ed );
//			TCOUT( "-- outEdge: " << edge );
			typename Edge::TimeMap::const_iterator timesNeeded = edge._timesNeeded.find( _time );
			if( timesNeeded != edge._timesNeeded.end() )
			{
				vertex._times.insert( (*timesNeeded).second.begin(), (*timesNeeded).second.end() );
//				std::cout << "--- insert vertex: ";
//				std::copy( (*timesNeeded).second.begin(),
//				           (*timesNeeded).second.end(),
//						   std::ostream_iterator<OfxTime>(std::cout, ",") );
//				std::cout << std::endl;
			}
		}
		
		// Set all times needed on each input edges
		BOOST_FOREACH( const OfxTime t, vertex._times )
		{
			TCOUT( "-  time: "<< t );
			INode::InputsTimeMap mapInputsTimes = vertex.getProcessNode()->getTimesNeeded( t );
//			BOOST_FOREACH( const INode::InputsTimeMap::value_type& v, mapInputsTimes )
//			{
//				TCOUT_VAR( v.first );
//			}
			BOOST_FOREACH( const edge_descriptor& ed, _graph.getOutEdges( v ) )
			{
				Edge& edge = _graph.instance( ed );
//				TCOUT( "-- inEdge "<<t<<": " << edge );
				const Vertex& input = _graph.targetInstance( ed );
//				TCOUT_VAR( input.getName() );
//				std::cout << "--- insert edges: ";
//				std::copy( mapInputsTimes[input.getName()+".Output"].begin(),
//				           mapInputsTimes[input.getName()+".Output"].end(),
//						   std::ostream_iterator<OfxTime>(std::cout, ",") );
				edge._timesNeeded[t] = mapInputsTimes[input.getName()+".Output"];
			}
		}

	}

private:
	TGraph& _graph;
	OfxTime _time;
};

template<class TGraph>
class PreProcess1 : public boost::default_dfs_visitor
{
public:
	typedef typename TGraph::GraphContainer GraphContainer;
	typedef typename TGraph::Vertex Vertex;

	PreProcess1( TGraph& graph )
		: _graph( graph )
	{}

	template<class VertexDescriptor, class Graph>
	void finish_vertex( VertexDescriptor v, Graph& g )
	{
		Vertex& vertex = _graph.instance( v );

		TCOUT( "[PREPROCESS 1] finish_vertex " << vertex );
		if( vertex.isFake() )
			return;

		vertex.getProcessNode()->preProcess1( vertex.getProcessOptions() );
	}

private:
	TGraph& _graph;
};

template<class TGraph>
class PreProcess2 : public boost::default_dfs_visitor
{
public:
	typedef typename TGraph::GraphContainer GraphContainer;
	typedef typename TGraph::Vertex Vertex;

	PreProcess2( TGraph& graph )
		: _graph( graph )
	{}

	template<class VertexDescriptor, class Graph>
	void finish_vertex( VertexDescriptor v, Graph& g )
	{
		Vertex& vertex = _graph.instance( v );

		TCOUT( "[PREPROCESS 2] finish_vertex " << vertex );
		if( vertex.isFake() )
			return;

		vertex.getProcessNode()->preProcess2_reverse( vertex.getProcessOptions() );
	}

private:
	TGraph& _graph;
};

template<class TGraph>
class PreProcess3 : public boost::default_dfs_visitor
{
public:
	typedef typename TGraph::GraphContainer GraphContainer;
	typedef typename TGraph::Vertex Vertex;

	PreProcess3( TGraph& graph )
		: _graph( graph )
	{}
	template<class VertexDescriptor, class Graph>
	void finish_vertex( VertexDescriptor v, Graph& g )
	{
		Vertex& vertex = _graph.instance( v );

		TCOUT( "[PREPROCESS 3] finish_vertex " << vertex );
		if( vertex.isFake() )
			return;

		vertex.getProcessNode()->preProcess3( vertex.getProcessOptions() );
	}

private:
	TGraph& _graph;
};

template<class TGraph>
class OptimizeGraph : public boost::default_dfs_visitor
{
public:
	typedef typename TGraph::GraphContainer GraphContainer;
	typedef typename TGraph::Vertex Vertex;
	typedef typename TGraph::Edge Edge;
	typedef typename TGraph::vertex_descriptor vertex_descriptor;
	typedef typename TGraph::edge_descriptor edge_descriptor;
	typedef typename TGraph::in_edge_iterator in_edge_iterator;
	typedef typename TGraph::out_edge_iterator out_edge_iterator;

	OptimizeGraph( TGraph& graph )
		: _graph( graph )
	{
//		COUT_X( 80, ":" );
	}

	template <class VertexDescriptor, class Graph>
	void finish_vertex( VertexDescriptor v, const Graph& g )
	{
		using namespace boost;
		using namespace boost::graph;
		Vertex& vertex = _graph.instance( v );

		ProcessOptions& procOptions = vertex.getProcessOptions();
		if( !vertex.isFake() )
		{
			// compute local infos, need to be a real node !
			vertex.getProcessNode()->preProcess_infos( procOptions._localInfos );
		}

		// compute global infos for inputs

		// direct dependencies (originally the node inputs)
		BOOST_FOREACH( const edge_descriptor& oe, out_edges( v, _graph.getGraph() ) )
		{
//			Edge& outEdge = _graph.instance(*oe);
			Vertex& outVertex = _graph.targetInstance( oe );
			procOptions._inputsInfos += outVertex.getProcessOptions()._localInfos;
			procOptions._globalInfos += outVertex.getProcessOptions()._globalInfos;
		}
		procOptions._globalInfos += procOptions._localInfos;

//		BOOST_FOREACH( const edge_descriptor& ie, in_edges( v, _graph.getGraph() ) )
//		{
//			Edge& e = _graph.instance( ie );
//		}


//		COUT_X( 80, "." );
//		COUT( vertex.getName() );
//		COUT( procOptions );
//		COUT_X( 80, "." );
	}

private:
	TGraph& _graph;
};

template<class TGraph>
class Process : public boost::default_dfs_visitor
{
public:
	typedef typename TGraph::GraphContainer GraphContainer;
	typedef typename TGraph::Vertex Vertex;

	Process( TGraph& graph, memory::MemoryCache& result )
		: _graph( graph )
		, _result( result )
	{
	}

	template<class VertexDescriptor, class Graph>
	void finish_vertex( VertexDescriptor v, Graph& g )
	{
		Vertex& vertex = _graph.instance( v );
//		COUT( "[PROCESS] finish_vertex " << vertex );

		// do nothing on the empty output node
		// it's just a link to final nodes
		if( vertex.isFake() )
			return;

		// check if abort ?

		// launch the process
		vertex.getProcessNode()->process( vertex.getProcessOptions() );
		
		if( vertex.getProcessOptions()._finalNode )
		{
//			_result.put( vertex._name, vertex._time, vertex._image);
		}
	}

private:
	TGraph& _graph;
	memory::MemoryCache& _result;
};

template<class TGraph>
class PostProcess : public boost::default_dfs_visitor
{
public:
	typedef typename TGraph::GraphContainer GraphContainer;
	typedef typename TGraph::Vertex Vertex;

	PostProcess( TGraph& graph )
		: _graph( graph )
	{}

	template<class VertexDescriptor, class Graph>
	void initialize_vertex( VertexDescriptor v, Graph& g )
	{
		Vertex& vertex = _graph.instance( v );

//		TCOUT( "[POSTPROCESS] initialize_vertex " << vertex );
		if( vertex.isFake() )
			return;
	}

	template<class VertexDescriptor, class Graph>
	void finish_vertex( VertexDescriptor v, Graph& g )
	{
		Vertex& vertex = _graph.instance( v );

//		TCOUT( "[POSTPROCESS] finish_vertex " << vertex );
		if( vertex.isFake() )
			return;

		vertex.getProcessNode()->postProcess( vertex.getProcessOptions() );
	}

private:
	TGraph& _graph;
};

}
}
}
}

#endif

