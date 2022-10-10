#include "ofxParameterTwister.h"

#include "ofMath.h"

#include "ofThreadChannel.h"
#include "ofParameter.h"

#include "RtMidi.h"
#include <array>

using namespace std;

static const std::string MIDI_DEVICE_NAME("Midi Fighter Twister");

/* We assume high resolution encoders with 14 bit resolution */
#define TW_MAX_ENCODER_VALUE (0x3FFF)

struct MidiCCMessage {

	uint8_t command_channel = 0xB0;
	uint8_t controller      = 0x00;
	uint8_t value           = 0x00;

	int getCommand() {
		// command is in the most significant 
		// 4 bits, so we shift 4 bits to the right.
		// e.g. 0xB0
		return command_channel >> 4;
	};

	int getChannel() {
		// channel is the least significant 4 bits,
		// so we null out the high bits
		return command_channel & 0x0F;
	};

};

// ------------------------------------------------------
/// \brief		static callback for midi controller
/// \detail		Translates the message into a midi messge object
///             and passes it on to a thread channel so it can be processed 
///             in update.
/// \note 
static void _midi_callback(double deltatime, std::vector< unsigned char > *message, void *threadChannel)
{

	auto tCh = static_cast<ofThreadChannel<MidiCCMessage>*>(threadChannel);

	// Default midi messages come in three bytes - any other messages will be ignored.

	if (message->size() == 3) {

		MidiCCMessage msg;
		msg.command_channel = message->at(0);
		msg.controller      = message->at(1);
		msg.value           = message->at(2);

#ifndef NDEBUG
		ostringstream ostr;
		ostr
			<< "midi message: "
			<< "0x" << std::hex << std::setw(2) << 1 * message->at(0) << " "
			<< "0x" << std::hex << std::setw(2) << 1 * message->at(1) << " "
			<< "0x" << std::hex << std::setw(2) << 1 * message->at(2)
			;
		ofLogVerbose() << ostr.str();
#endif
		
		tCh->send(std::move(msg));

	}
}

struct Encoder {

	RtMidiOut*	mMidiOut = nullptr;

	// Position on the controller left to right, top to bottom (row-major)
	uint8_t pos = 0;

	bool rotaryEnabled = false;
	bool switchEnabled = false;

	// Internal hardware representation of the knob value 
	// may be (0..3FFF) == (0..2^14-1) == (0..16383)

	// event listener for parameter change
	ofEventListener mELSwitchParamChange;
	ofEventListener mELRotaryParamChange;

	std::function<void(uint8_t v_, uint8_t hr_)> updateSwitchParam;
	std::function<void(uint8_t v_, uint8_t hr_)> updateRotaryParam;

	void setRotaryState(bool enabled_, bool force_ = false);
	void setRotaryValue(uint16_t value);

	void setSwitchState(bool enabled_, bool force_ = false);
	void setSwitchValue(uint16_t value);

	void sendToSwitch(uint8_t v_);
	void sendToRotary(uint8_t msb, uint8_t lsb); // most significant byte, least significant byte

	void setEncoderPhenotype(uint8_t phenotype);

	void setHueRGB(float h_);
	void setBrightnessRGB(float b_);
	void setBrightnessRotary(float b_); /// brightness is normalised over 31 steps 0..30

	void setAnimation(uint8_t v_);
};

// Implementation for paramter twister - any calls to twister will be forwared to 
// an implementation instance.

class ofxParameterTwisterImpl {
	
	RtMidiIn*	mMidiIn = nullptr;
	RtMidiOut*	mMidiOut = nullptr;

	ofThreadChannel<MidiCCMessage> mChannelMidiIn;

	ofParameterGroup mParams;

	std::array<Encoder, 16> mEncoders;

	uint8_t mHighResVelLowByte = 0; // last low byte signal received via `Bn 58 vv` message (reset after each CC message)

public:
	void setup();
	void clear();

	void setParams(const ofParameterGroup& group_);

	void setParam(size_t idx_, ofParameter<float>& param_);
	void setParam(size_t idx_, ofParameter<int>& param_);
	void setParam(size_t idx_, ofParameter<bool>& param_);
	void clearParam(size_t idx_, bool force_ = false);

	void setParam(Encoder& encoder_, ofParameter<float>& param_);
	void setParam(Encoder& encoder_, ofParameter<int>& param_);
	void setParam(Encoder& encoder_, ofParameter<bool>& param_);
	void clearParam(Encoder& encoder_, bool force_);

	void setHueRGB(size_t idx_, float hue_);
	void setBrightnessRGB(size_t idx_, float bri_);
	void setAnimationRGB(size_t idx_, ofxParameterTwister::Animation anim_, uint8_t rate_ = 0);

	void setBrightnessRotary(size_t idx_, float bri_);
	void setAnimationRotary(size_t idx_, ofxParameterTwister::Animation anim_, uint8_t rate_ = 0);

	void setHueRGB(Encoder& encoder_, float hue_);
	void setBrightnessRGB(Encoder& encoder_, float bri_);
	void setAnimationRGB(Encoder& encoder_, ofxParameterTwister::Animation anim_, uint8_t rate_);

	void setBrightnessRotary(Encoder& encoder_, float bri_);
	void setAnimationRotary(Encoder& encoder_, ofxParameterTwister::Animation anim_, uint8_t rate_);

	void update();
	~ofxParameterTwisterImpl();
};

// ------------------------------------------------------

ofxParameterTwister::ofxParameterTwister() {
}

// ------------------------------------------------------

ofxParameterTwister::~ofxParameterTwister() {
}

// ------------------------------------------------------

void ofxParameterTwister::setup() {
	impl = std::make_unique<ofxParameterTwisterImpl>();
	impl->setup();
}

// ------------------------------------------------------

void ofxParameterTwister::clear() {
	if (impl) {
		impl->clear();
	}
}

// ------------------------------------------------------

void ofxParameterTwister::setParams(const ofParameterGroup& group_) {
	
	if (impl) {
		impl->setParams(group_);
	}
	else {
		ofLogWarning() << "ofxParameterTwister::" << __func__ << "() : setup() must be called before calling " << __func__ << " for the first time. Calling setup implicitly...";
		setup();
		// call this method again.
		setParams(group_);
	}
	
}

// ------------------------------------------------------

void ofxParameterTwister::setParam(size_t idx_, ofParameter<float>& param_) {
	if (impl) {
		impl->setParam(idx_, param_);
	}
	else {
		ofLogWarning() << "ofxParameterTwister::" << __func__ << "() : setup() must be called before calling " << __func__ << " for the first time. Calling setup implicitly...";
		setup();
		// call this method again.
		setParam(idx_, param_);
	}
}

// ------------------------------------------------------

void ofxParameterTwister::setParam(size_t idx_, ofParameter<int>& param_) {
	if (impl) {
		impl->setParam(idx_, param_);
	}
	else {
		ofLogWarning() << "ofxParameterTwister::" << __func__ << "() : setup() must be called before calling " << __func__ << " for the first time. Calling setup implicitly...";
		setup();
		// call this method again.
		setParam(idx_, param_);
	}
}

// ------------------------------------------------------

void ofxParameterTwister::setParam(size_t idx_, ofParameter<bool>& param_) {
	if (impl) {
		impl->setParam(idx_, param_);
	}
	else {
		ofLogWarning() << "ofxParameterTwister::" << __func__ << "() : setup() must be called before calling " << __func__ << " for the first time. Calling setup implicitly...";
		setup();
		// call this method again.
		setParam(idx_, param_);
	}
}

// ------------------------------------------------------

void ofxParameterTwister::clearParam(size_t idx_, bool force_) {
	if (impl) {
		impl->clearParam(idx_, force_);
	}
	else {
		ofLogWarning() << "ofxParameterTwister::" << __func__ << "() : setup() must be called before calling " << __func__ << " for the first time. Calling setup implicitly...";
		setup();
		// call this method again.
		clearParam(idx_, force_);
	}
}

// ------------------------------------------------------

void ofxParameterTwister::setHueRGB(size_t idx_, float hue_) {
	if (impl) {
		impl->setHueRGB(idx_, hue_);
	}
	else {
		ofLogWarning() << "ofxParameterTwister::" << __func__ << "() : setup() must be called before calling " << __func__ << " for the first time. Calling setup implicitly...";
		setup();
		// call this method again.
		setHueRGB(idx_, hue_);
	}
}

// ------------------------------------------------------

void ofxParameterTwister::setBrightnessRGB(size_t idx_, float bri_) {
	if (impl) {
		impl->setBrightnessRGB(idx_, bri_);
	}
	else {
		ofLogWarning() << "ofxParameterTwister::" << __func__ << "() : setup() must be called before calling " << __func__ << " for the first time. Calling setup implicitly...";
		setup();
		// call this method again.
		setBrightnessRGB(idx_, bri_);
	}
}

// ------------------------------------------------------

void ofxParameterTwister::setAnimationRGB(size_t idx_, Animation anim_, uint8_t rate_) {
	if (impl) {
		impl->setAnimationRGB(idx_, anim_, rate_);
	}
	else {
		ofLogWarning() << "ofxParameterTwister::" << __func__ << "() : setup() must be called before calling " << __func__ << " for the first time. Calling setup implicitly...";
		setup();
		// call this method again.
		setAnimationRGB(idx_, anim_, rate_);
	}
}

// ------------------------------------------------------

void ofxParameterTwister::setBrightnessRotary(size_t idx_, float bri_) {
	if (impl) {
		impl->setBrightnessRotary(idx_, bri_);
	}
	else {
		ofLogWarning() << "ofxParameterTwister::" << __func__ << "() : setup() must be called before calling " << __func__ << " for the first time. Calling setup implicitly...";
		setup();
		// call this method again.
		setBrightnessRotary(idx_, bri_);
	}
}

// ------------------------------------------------------

void ofxParameterTwister::setAnimationRotary(size_t idx_, Animation anim_, uint8_t rate_) {
	if (impl) {
		impl->setAnimationRotary(idx_, anim_, rate_);
	}
	else {
		ofLogWarning() << "ofxParameterTwister::" << __func__ << "() : setup() must be called before calling " << __func__ << " for the first time. Calling setup implicitly...";
		setup();
		// call this method again.
		setAnimationRotary(idx_, anim_, rate_);
	}
}

// ------------------------------------------------------

void ofxParameterTwister::update() {

	if (impl) {
		impl->update();
	}
	else {
		ofLogWarning() << "ofxParameterTwister::" << __func__ << "() : setup() must be called before calling " << __func__ << " for the first time. Calling setup implicitly...";
		setup();
		// call this method again.
		update();
	}

}

// ------------------------------------------------------

void Encoder::setRotaryState(bool enabled_, bool force_) {
	if (rotaryEnabled == enabled_ && force_ == false) {
		return;
	}

	// ----------| invariant: state change requested, or forced

	if (enabled_) {
		setEncoderPhenotype(0);
	}
	else if (!switchEnabled) {
		setEncoderPhenotype(2);
	}

	rotaryEnabled = enabled_;
}

// ------------------------------------------------------

void Encoder::setRotaryValue(uint16_t v_) {
	if (!rotaryEnabled) {
		ofLogError() << "Cannot send value to disabled encoder" << pos;
		return;
	}

	uint8_t msb = ((v_ >> 7) & 0x7F); // most significant byte
	uint8_t lsb = (v_ & 0x7F);		  // least significant byte
	sendToRotary(msb, lsb);
}

// ------------------------------------------------------

void Encoder::setSwitchState(bool enabled_, bool force_) {
	if (switchEnabled == enabled_ && force_ == false) {
		return;
	}

	// ----------| invariant: state change requested, or forced

	if (enabled_) {
		setEncoderPhenotype(1);
	}
	else if (!rotaryEnabled) {
		setEncoderPhenotype(2);
	}

	switchEnabled = enabled_;
}

// ------------------------------------------------------

void Encoder::setSwitchValue(uint16_t v_) {
	if (!switchEnabled) {
		ofLogError() << "Cannot send value to disabled encoder" << pos;
		return;
	}

	uint8_t msb = ((v_ >> 7) & 0x7F); // most significant byte
	uint8_t lsb = (v_ & 0x7F);		  // least significant byte
	sendToSwitch(msb);
}

// ------------------------------------------------------

void Encoder::sendToSwitch(uint8_t v_) {
	if (mMidiOut == nullptr)
		return;

	// ----------| invariant: midiOut is not nullptr

	vector<unsigned char> msg{
		0xB1,					// `B` means "Control Change" message, lower nibble means channel id; SWITCH listens on channel 1
		pos,					// device id
		v_,						// value
	};
	mMidiOut->sendMessage(&msg);

	ofLogVerbose() << ">>" << setw(2) << 1 * pos << " SWI " << " : " << setw(3) << v_ * 1;
}

// ------------------------------------------------------

void Encoder::sendToRotary(uint8_t msb, uint8_t lsb) {
	if (mMidiOut == nullptr)
		return;

	// ----------| invariant: midiOut is not nullptr

	vector<unsigned char> msgHiresPrefix{
		0xB0,
		0x58,
		lsb,
	};

	mMidiOut->sendMessage(&msgHiresPrefix);
	
	vector<unsigned char> msg{
		0xB0,					// `B` means "Control Change" message, lower nibble means channel id; ROTARY listens on channel 0
		pos,					// device id
		msb,					// value
	};

	mMidiOut->sendMessage(&msg);

	ofLogVerbose() << ">>" << setw(2) << 1 * pos << " ROT " << " : " << setw(3) << msb * 1;
}

// ------------------------------------------------------

void Encoder::setEncoderPhenotype(uint8_t phenotype)
{
	if (mMidiOut == nullptr)
		return;

	// ----------| invariant: midiOut is not nullptr

	vector<unsigned char> msg{
		0xB4,					// `B` means "Control Change" message, lower nibble means channel id; ENCODER_CONTROL messages are set via channel 4
		pos,					// device id
		phenotype,
	};

	mMidiOut->sendMessage(&msg);

}

// ------------------------------------------------------

void Encoder::setHueRGB(float h_)
{
	if (mMidiOut == nullptr)
		return;

	// ----------| invariant: midiOut is not nullptr

	unsigned char val = std::roundf(ofMap(h_, 0.f, 1.f, 1, 126, true));

	vector<unsigned char> msg{
		0xB1,					// `B` means "Control Change" message, lower nibble means channel id; COLOR_CONTROL messages are set via channel 1
		pos,					// device id
		val,
	};

	mMidiOut->sendMessage(&msg);

}

// ------------------------------------------------------

void Encoder::setBrightnessRotary(float b_)
{
	if (mMidiOut == nullptr)
		return;

	// ----------| invariant: midiOut is not nullptr

	unsigned char val = std::round(ofMap(b_, 0.f, 1.f, 65, 95, true));
	
	vector<unsigned char> msg{
		0xB2,					// `B` means "Control Change" message, lower nibble means channel id; ANIMATIONS are set via channel 2
		pos,					// device id
		val,
	};

	mMidiOut->sendMessage(&msg);

}

 //------------------------------------------------------

void Encoder::setBrightnessRGB(float b_)
{
	if (mMidiOut == nullptr)
		return;

	// ----------| invariant: midiOut is not nullptr

	unsigned char val = round(ofMap(b_, 0.f, 1.f, 17, 47, true));

	vector<unsigned char> msg{
		0xB2,					// // `B` means "Control Change" message, lower nibble means channel id; ANIMATIONS are set via channel 2 
		pos,					// device id
		val,
	};

	mMidiOut->sendMessage(&msg);

}
// ------------------------------------------------------

void Encoder::setAnimation(uint8_t v_)
{
	if (mMidiOut == nullptr)
		return;

	// ----------| invariant: midiOut is not nullptr

	vector<unsigned char> msg{
		0xB2,					// `B` means "Control Change" message, lower nibble means channel id; ANIMATIONS are set via channel 2
		pos,					// device id
		v_,
	};

	mMidiOut->sendMessage(&msg);

}

// ------------------------------------------------------

ofxParameterTwisterImpl::~ofxParameterTwisterImpl() {
	if (mMidiIn != nullptr) {
		mMidiIn->closePort();
		delete mMidiIn;
		mMidiIn = nullptr;
	}
	if (mMidiOut != nullptr) {
		mMidiOut->closePort();
		delete mMidiOut;
		mMidiOut = nullptr;
	}
}

// ------------------------------------------------------

void ofxParameterTwisterImpl::setup() {
	// establish midi in connection,
	// and bind callback for midi in.
	try {
		mMidiIn = new RtMidiIn();
		size_t numPorts = mMidiIn->getPortCount();
		size_t midiPort = -1;
		
		if (mMidiIn->getPortCount() >= 1)
		{
			for (size_t i = 0; i < numPorts; ++i)
			{
				if (mMidiIn->getPortName(i).substr(0, MIDI_DEVICE_NAME.size()) == MIDI_DEVICE_NAME)
				{
					midiPort = i;
					mMidiIn->openPort(midiPort);
					mMidiIn->setCallback(&_midi_callback, &mChannelMidiIn);

					// Don't ignore sysex, timing, or active sensing messages.
					mMidiIn->ignoreTypes(true, true, true);
				}
			}
		}
	}
	catch (RtMidiError &error)
	{
		std::cout << "MIDI input exception:" << std::endl;
		error.printMessage();
	}

	// establish midi out connection
	try {
		mMidiOut = new RtMidiOut();
		size_t numPorts = mMidiOut->getPortCount();
		size_t midiPort = -1;
		if (mMidiOut->getPortCount() >= 1)
		{
			for (size_t i = 0; i < numPorts; ++i)
			{
				if (mMidiOut->getPortName(i).substr(0, MIDI_DEVICE_NAME.size()) == MIDI_DEVICE_NAME)
				{
					midiPort = i;
					mMidiOut->openPort(midiPort);
				}
			}
		}
	}
	catch (RtMidiError &error)
	{
		std::cout << "MIDI output exception:" << std::endl;
		error.printMessage();
	}

	// assign ids to encoders
	for (int i = 0; i < 16; ++i) {
		mEncoders[i].pos = i;
		mEncoders[i].mMidiOut = mMidiOut;
	};
}

// ------------------------------------------------------

void ofxParameterTwisterImpl::clear() {
	for (auto & e : mEncoders) {
		clearParam(e, true);
	}
}

// ------------------------------------------------------

void ofxParameterTwisterImpl::setParams(const ofParameterGroup & group_) {
	ofLogVerbose() << "Updating mapping" << endl;
	/*

	based on incoming parameters,
	we set our Encoders to track them

	*/

	auto it = group_.begin();
	auto endIt = group_.end();

	for (auto & e : mEncoders) {

		if (it != endIt) {
			if (auto param = dynamic_pointer_cast<ofParameter<float>>(*it)) {
				// bingo, we have a float param
				setParam(e, *param);

			}
			else if (auto param = dynamic_pointer_cast<ofParameter<int>>(*it)) {
				// bingo, we have a float param
				setParam(e, *param);

			}
			else if (auto param = dynamic_pointer_cast<ofParameter<bool>>(*it)) {
				// we have a bool parameter
				setParam(e, *param);

			}
			else {
				// we cannot match this parameter, unfortunately
				clearParam(e, false);
			}

			it++;

		}
		else {
			// no more parameters to map.
			e.setRotaryState(false, true);
			e.setSwitchState(false, true);
		}
	}
}

// ------------------------------------------------------

void ofxParameterTwisterImpl::setParam(size_t idx_, ofParameter<float>& param_) {
	if (idx_ < mEncoders.size()) {
		setParam(mEncoders[idx_], param_);
	}
}

// ------------------------------------------------------

void ofxParameterTwisterImpl::setParam(Encoder& encoder_, ofParameter<float>& param_) {
	encoder_.setRotaryState(true);
	encoder_.setRotaryValue(ofMap(param_, param_.getMin(), param_.getMax(), 0, TW_MAX_ENCODER_VALUE, true));

	// now set the Encoder's event listener to track 
	// this parameter
	auto pMin = param_.getMin();
	auto pMax = param_.getMax();

	encoder_.updateRotaryParam = [&param_, pMin, pMax](uint8_t msb, uint8_t lsb) {
		uint16_t highRezVal = ((msb & 0x7F) << 7) | (lsb & 0x7F);
		// on midi input
		param_.set(ofMap(highRezVal, 0, TW_MAX_ENCODER_VALUE, pMin, pMax, true));
	};

	encoder_.mELRotaryParamChange = param_.newListener([&encoder_, pMin, pMax](float v_) {
		// on parameter change, write from parameter 
		// to midi.
		encoder_.setRotaryValue(ofMap(v_, pMin, pMax, 0, TW_MAX_ENCODER_VALUE, true));
	});
}

// ------------------------------------------------------

void ofxParameterTwisterImpl::setParam(size_t idx_, ofParameter<int>& param_) {
	if (idx_ < mEncoders.size()) {
		setParam(mEncoders[idx_], param_);
	}
}

// ------------------------------------------------------

void ofxParameterTwisterImpl::setParam(Encoder& encoder_, ofParameter<int>& param_) {
	encoder_.setRotaryState(true);
	encoder_.setRotaryValue(ofMap(param_, param_.getMin(), param_.getMax(), 0, TW_MAX_ENCODER_VALUE, true));

	// now set the Encoder's event listener to track 
	// this parameter
	auto pMin = param_.getMin();
	auto pMax = param_.getMax();

	encoder_.updateRotaryParam = [&param_, pMin, pMax](uint8_t msb, uint8_t lsb) {
		uint16_t highRezVal = ((msb & 0x7F) << 7) | (lsb & 0x7F);
		// on midi input
		param_.set(ofMap(highRezVal, 0, TW_MAX_ENCODER_VALUE, pMin, pMax, true));
	};

	encoder_.mELRotaryParamChange = param_.newListener([&encoder_, pMin, pMax](int v_) {
		// on parameter change, write from parameter 
		// to midi.
		encoder_.setRotaryValue(ofMap(v_, pMin, pMax, 0, TW_MAX_ENCODER_VALUE, true));
		});
}

// ------------------------------------------------------

void ofxParameterTwisterImpl::setParam(size_t idx_, ofParameter<bool>& param_) {
	if (idx_ < mEncoders.size()) {
		setParam(mEncoders[idx_], param_);
	}
}

// ------------------------------------------------------

void ofxParameterTwisterImpl::setParam(Encoder& encoder_, ofParameter<bool>& param_) {
	encoder_.setSwitchState(true);
	encoder_.setSwitchValue((param_ == true) ? TW_MAX_ENCODER_VALUE : 0);

	encoder_.updateSwitchParam = [&param_](uint8_t msb, uint8_t lsb) {
		param_.set((msb > 63) ? true : false);
	};

	encoder_.mELSwitchParamChange = param_.newListener([&encoder_](bool v_) {
		encoder_.setSwitchValue(v_ == true ? TW_MAX_ENCODER_VALUE : 0);
		});
}

// ------------------------------------------------------

void ofxParameterTwisterImpl::clearParam(size_t idx_, bool force_) {
	if (idx_ < mEncoders.size()) {
		clearParam(mEncoders[idx_], force_);
	}
}

// ------------------------------------------------------

void ofxParameterTwisterImpl::clearParam(Encoder& encoder_, bool force_) {
	encoder_.setRotaryState(false, force_);
	encoder_.mELRotaryParamChange.unsubscribe(); // reset listener

	encoder_.setSwitchState(false, force_);
	encoder_.mELSwitchParamChange.unsubscribe(); // reset listener
}

// ------------------------------------------------------

void ofxParameterTwisterImpl::setHueRGB(size_t idx_, float hue_)
{
	if (idx_ < mEncoders.size())
	{
		setHueRGB(mEncoders[idx_], hue_);
	}
}

// ------------------------------------------------------

void ofxParameterTwisterImpl::setHueRGB(Encoder& encoder_, float hue_)
{
	encoder_.setHueRGB(hue_);
}

// ------------------------------------------------------

void ofxParameterTwisterImpl::setBrightnessRGB(size_t idx_, float bri_)
{
	if (idx_ < mEncoders.size())
	{
		setBrightnessRGB(mEncoders[idx_], bri_);
	}
}

// ------------------------------------------------------

void ofxParameterTwisterImpl::setBrightnessRGB(Encoder& encoder_, float bri_)
{
	encoder_.setBrightnessRGB(bri_);
}

// ------------------------------------------------------

void ofxParameterTwisterImpl::setAnimationRGB(size_t idx_, ofxParameterTwister::Animation anim_, uint8_t rate_)
{
	if (idx_ < mEncoders.size() && rate_ < 8)
	{
		setAnimationRGB(mEncoders[idx_], anim_, rate_);
	}
}

// ------------------------------------------------------

void ofxParameterTwisterImpl::setAnimationRGB(Encoder& encoder_, ofxParameterTwister::Animation anim_, uint8_t rate_)
{
	uint8_t mode;
	switch (anim_)
	{
	case ofxParameterTwister::Animation::NONE:
		mode = 0;
		break;
	case ofxParameterTwister::Animation::STROBE:
		mode = 1 + rate_;
		break;
	case ofxParameterTwister::Animation::PULSE:
		mode = 9 + rate_;
		break;
	case ofxParameterTwister::Animation::RAINBOW:
	default:
		mode = 127;
	}
	encoder_.setAnimation(mode);
}

// ------------------------------------------------------

void ofxParameterTwisterImpl::setBrightnessRotary(size_t idx_, float bri_)
{
	if (idx_ < mEncoders.size())
	{
		setBrightnessRotary(mEncoders[idx_], bri_);
	}
}

// ------------------------------------------------------

void ofxParameterTwisterImpl::setBrightnessRotary(Encoder& encoder_, float bri_)
{
	encoder_.setBrightnessRotary(bri_);
}

// ------------------------------------------------------

void ofxParameterTwisterImpl::setAnimationRotary(size_t idx_, ofxParameterTwister::Animation anim_, uint8_t rate_)
{
	if (idx_ < mEncoders.size() && rate_ < 8 && anim_ != ofxParameterTwister::Animation::RAINBOW)
	{
		setAnimationRotary(mEncoders[idx_], anim_, rate_);
	}
}

// ------------------------------------------------------

void ofxParameterTwisterImpl::setAnimationRotary(Encoder& encoder_, ofxParameterTwister::Animation anim_, uint8_t rate_)
{
	uint8_t mode;
	switch (anim_)
	{
	case ofxParameterTwister::Animation::NONE:
		mode = 48;
		break;
	case ofxParameterTwister::Animation::STROBE:
		mode = 49 + rate_;
		break;
	case ofxParameterTwister::Animation::PULSE:
	default:
		mode = 57 + rate_;
		break;
	}
	encoder_.setAnimation(mode);
}

// ------------------------------------------------------

void ofxParameterTwisterImpl::update() {
	MidiCCMessage m;

	while (mChannelMidiIn.tryReceive(m)) {

		// we got a message.
		// let's get the address.

		if (m.getCommand() == 0xB) {

			if (m.getChannel() == 0x0) {
				// rotary message
				uint8_t encoderID = m.controller;

				if (m.controller == 0x58) { // high rez velocity message
					
					mHighResVelLowByte = (m.value & 0x7F); // limit to lower 7 bits, 0..127

				} else if (encoderID < mEncoders.size()){

					auto &e = mEncoders[encoderID];
					if (e.rotaryEnabled) {

						// write value back to parameter
						if (e.updateRotaryParam) {
							e.updateRotaryParam(m.value, mHighResVelLowByte);
						}
						
					}

					mHighResVelLowByte = 0;
				}
			}else 

			if (m.getChannel() == 0x1) {
				// rotary message
				size_t encoderID = m.controller;
				auto &e = mEncoders[encoderID];
				if (e.switchEnabled)
					if (e.updateSwitchParam)
						e.updateSwitchParam(m.value,0);
			}
		}
	}
}

// ------------------------------------------------------
