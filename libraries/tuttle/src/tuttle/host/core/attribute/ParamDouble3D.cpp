#include "ParamDouble3D.hpp"

namespace tuttle {
namespace host {
namespace core {

ParamDouble3D::ParamDouble3D( ImageEffectNode& effect,
                              const std::string& name,
                              const ofx::attribute::OfxhParamDescriptor& descriptor )
	: ofx::attribute::OfxhMultiDimParam<ParamDouble, 3>( descriptor, name, effect ),
	_effect( effect )

{
	_controls.push_back( new ParamDouble( effect, name + ".x", descriptor, 0 ) );
	_controls.push_back( new ParamDouble( effect, name + ".y", descriptor, 1 ) );
	_controls.push_back( new ParamDouble( effect, name + ".z", descriptor, 2 ) );
}

Ofx3DPointD ParamDouble3D::getDefault() const
{
	Ofx3DPointD point;

	point.x = _controls[0]->getDefault();
	point.y = _controls[1]->getDefault();
	point.z = _controls[2]->getDefault();
	return point;
}

void ParamDouble3D::get( double& x, double& y, double& z ) const OFX_EXCEPTION_SPEC
{
	_controls[0]->get(x);
	_controls[1]->get(y);
	_controls[2]->get(z);
}

void ParamDouble3D::get( const OfxTime time, double& x, double& y, double& z ) const OFX_EXCEPTION_SPEC
{
	_controls[0]->get(time, x);
	_controls[1]->get(time, y);
	_controls[2]->get(time, z);
}

void ParamDouble3D::set( const double &x, const double &y, const double &z ) OFX_EXCEPTION_SPEC
{
	_controls[0]->set(x);
	_controls[1]->set(y);
	_controls[2]->set(z);
}

void ParamDouble3D::set( const OfxTime time, const double &x, const double &y, const double &z ) OFX_EXCEPTION_SPEC
{
	_controls[0]->set(time, x);
	_controls[1]->set(time, y);
	_controls[2]->set(time, z);
}


}
}
}

