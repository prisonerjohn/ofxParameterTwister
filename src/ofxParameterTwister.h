#pragma once

template<typename ParameterType> class ofParameter; //ffdecl.
class ofParameterGroup; //ffdecl.
class ofxParameterTwisterImpl; // ffdecl.

#include <memory>

/*

+ internally, we hold a parametergroup

  + we can set a parameter group
  + re can clear a parameter group

  up to 16 parameters from the parameter group
  bind/unbind automatically to twister:

  float -> rotary controller
  bool  -> switch (button)

  button state is represented using RGB color.
  by default, buttons act as toggles.

  parameter change outside of twister is sent to twister
  whilst parameters are bound to twister.

*/

class ofxParameterTwister
{
	std::unique_ptr<ofxParameterTwisterImpl> impl;

public:
	
	ofxParameterTwister();
	~ofxParameterTwister();

	void setup();
	void clear();

	/// Call this method once per frame to read back Midi values from Twister
	void update(); 

	void setParams(const ofParameterGroup& group_);

	void setParam(size_t idx_, ofParameter<float>& param_);
	void setParam(size_t idx_, ofParameter<bool>& param_);
	void clearParam(size_t idx_, bool force_ = false);

};


