#include "plugin.hpp"

const float MIN_TIME = 1e-3f;
const float MAX_TIME = 10.f;
const float LAMBDA_BASE = MAX_TIME / MIN_TIME;

struct Envelope_1 : Module {
	enum ParamIds {
		STARTKNOB_PARAM,
		ATTACKKNOB_PARAM,
		ATTACKSLOPEKNOB_PARAM,
		HOLDKNOB_PARAM,
		DECAYKNOB_PARAM,
		DECAYSLOPEKNOB_PARAM,
		SUSTAINKNOB_PARAM,
		DELAYKNOB_PARAM,
		RELEASEKNOB_PARAM,
		RELEASESLOPEKNOB_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		STARTJACK_INPUT,
		ATTACKJACK_INPUT,
		HOLDJACK_INPUT,
		DECAYJACK_INPUT,
		SUSTAINJACK_INPUT,
		DELAYJACK_INPUT,
		RELEASEJACK_INPUT,
		GATEJACK_INPUT,
		TRIGJACK_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENVELOPEJACK_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		STARTLED_LIGHT,
		ATTACKLED_LIGHT,
		HOLDLED_LIGHT,
		DECAYLED_LIGHT,
		SUSTAINLED_LIGHT,
		DELAYLED_LIGHT,
		RELEASELED_LIGHT,
		NUM_LIGHTS
	};

	simd::float_4 attacking[4] = {simd::float_4::zero()};
	simd::float_4 env[4] = {0.f};
	dsp::TSchmittTrigger<simd::float_4> trigger[4];
	dsp::ClockDivider cvDivider;

	simd::float_4 startValueLambda[4] = {0.f};
	simd::float_4 attackValueLambda[4] = {0.f};
	simd::float_4 holdValueLambda[4] = {0.f};
	simd::float_4 decayValueLambda[4] = {0.f};
	simd::float_4 sustainValueLambda[4] = {0.f};
	simd::float_4 delayValueLambda[4] = {0.f};
	simd::float_4 releaseValueLambda[4] = {0.f};

	dsp::ClockDivider lightDivider;

	Envelope_1() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(STARTKNOB_PARAM, 0.f, 1.f, 0.5f, "Start", " ms", LAMBDA_BASE, MIN_TIME * 1000);
		configParam(ATTACKKNOB_PARAM, 0.f, 1.f, 0.5f, "Attack", " ms", LAMBDA_BASE, MIN_TIME * 1000);
		configParam(ATTACKSLOPEKNOB_PARAM, 0.f, 1.f, 0.5f, "Attack slope", "", 0, 100);
		configParam(HOLDKNOB_PARAM, 0.f, 1.f, 0.5f, "Hold", " ms", LAMBDA_BASE, MIN_TIME * 1000);
		configParam(DECAYKNOB_PARAM, 0.f, 1.f, 0.5f, "Decay", " ms", LAMBDA_BASE, MIN_TIME * 1000);
		configParam(DECAYSLOPEKNOB_PARAM, 0.f, 1.f, 0.5f, "Decay slope", "", 0, 100);
		configParam(SUSTAINKNOB_PARAM, 0.f, 1.f, 0.5f, "Sustain", "%", 0, 100);
		configParam(DELAYKNOB_PARAM, 0.f, 1.f, 0.5f, "Decay", " ms", LAMBDA_BASE, MIN_TIME * 1000);
		configParam(RELEASEKNOB_PARAM, 0.f, 1.f, 0.5f, "Release", " ms", LAMBDA_BASE, MIN_TIME * 1000);
		configParam(RELEASESLOPEKNOB_PARAM, 0.f, 1.f, 0.5f, "Release slope", "", 0, 100);

		cvDivider.setDivision(16);
		lightDivider.setDivision(128);
	}

	void process(const ProcessArgs& args) override {
		// 0.16-0.19 us serial
		// 0.23 us serial with all lambdas computed
		// 0.15-0.18 us serial with all lambdas computed with SSE

		int channels = inputs[GATEJACK_INPUT].getChannels();

		// Compute lambdas
		if (cvDivider.process()) {
			float startParamValue = params[STARTKNOB_PARAM].getValue();
			float attackParamValue = params[ATTACKKNOB_PARAM].getValue();
			float holdParamValue = params[HOLDKNOB_PARAM].getValue();
			float decayParamValue = params[DECAYKNOB_PARAM].getValue();
			float sustainParamValue = params[SUSTAINKNOB_PARAM].getValue();
			float delayParamValue = params[DELAYKNOB_PARAM].getValue();
			float releaseParamValue = params[RELEASEKNOB_PARAM].getValue();

			float attackSlopeParamValue = params[ATTACKSLOPEKNOB_PARAM].getValue();
			float decaySlopeParamValue = params[DECAYSLOPEKNOB_PARAM].getValue();
			float releaseSlopeParamValue = params[RELEASESLOPEKNOB_PARAM].getValue();

			for (int channel = 0; channel < channels; channel += 4) {
				// Start
				simd::float_4 startValue = startParamValue;
				if (inputs[STARTJACK_INPUT].isConnected())
					startValue += inputs[STARTJACK_INPUT].getPolyVoltageSimd<simd::float_4>(channel) / 10.f;
				startValue = simd::clamp(startValue, 0.f, 1.f);
				startValueLambda[channel / 4] = simd::pow(LAMBDA_BASE, -startValue) / MIN_TIME;

				// Attack
				simd::float_4 attackValue = attackParamValue;
				if (inputs[ATTACKJACK_INPUT].isConnected())
					attackValue += inputs[ATTACKJACK_INPUT].getPolyVoltageSimd<simd::float_4>(channel) / 10.f;
				attackValue = simd::clamp(attackValue, 0.f, 1.f);
				attackValueLambda[channel / 4] = simd::pow(LAMBDA_BASE, -attackValue) / MIN_TIME;

				// Hold
				simd::float_4 holdValue = holdParamValue;
				if (inputs[HOLDJACK_INPUT].isConnected())
					holdValue += inputs[HOLDJACK_INPUT].getPolyVoltageSimd<simd::float_4>(channel) / 10.f;
				holdValue = simd::clamp(holdValue, 0.f, 1.f);
				holdValueLambda[channel / 4] = simd::pow(LAMBDA_BASE, -holdValue) / MIN_TIME;

				// Decay
				simd::float_4 decayValue = decayParamValue;
				if (inputs[DECAYJACK_INPUT].isConnected())
					decayValue += inputs[DECAYJACK_INPUT].getPolyVoltageSimd<simd::float_4>(channel) / 10.f;
				decayValue = simd::clamp(decayValue, 0.f, 1.f);
				decayValueLambda[channel / 4] = simd::pow(LAMBDA_BASE, -decayValue) / MIN_TIME;

				// Sustain
				simd::float_4 sustainValue = sustainParamValue;
				if (inputs[SUSTAINJACK_INPUT].isConnected())
					sustainValue += inputs[SUSTAINJACK_INPUT].getPolyVoltageSimd<simd::float_4>(channel) / 10.f;
				sustainValue = simd::clamp(sustainValue, 0.f, 1.f);
				sustainValueLambda[channel / 4] = simd::pow(LAMBDA_BASE, -sustainValue) / MIN_TIME;

				// Delay
				simd::float_4 delayValue = delayParamValue;
				if (inputs[DELAYJACK_INPUT].isConnected())
					delayValue += inputs[DELAYJACK_INPUT].getPolyVoltageSimd<simd::float_4>(channel) / 10.f;
				delayValue = simd::clamp(delayValue, 0.f, 1.f);
				delayValueLambda[channel / 4] = simd::pow(LAMBDA_BASE, -delayValue) / MIN_TIME;

				// Release
				simd::float_4 releaseValue = releaseParamValue;
				if (inputs[RELEASEJACK_INPUT].isConnected())
					releaseValue += inputs[RELEASEJACK_INPUT].getPolyVoltageSimd<simd::float_4>(channel) / 10.f;
				releaseValue = simd::clamp(releaseValue, 0.f, 1.f);
				releaseValueLambda[channel / 4] = simd::pow(LAMBDA_BASE, -releaseValue) / MIN_TIME;
			}
		}

		simd::float_4 gate[4];

		for (int c = 0; c < channels; c += 4) {
			// Gate
			gate[c / 4] = inputs[GATEJACK_INPUT].getVoltageSimd<simd::float_4>(c) >= 1.f;

			// Retrigger
			simd::float_4 triggered = trigger[c / 4].process(inputs[TRIGJACK_INPUT].getPolyVoltageSimd<simd::float_4>(c));
			attacking[c / 4] = simd::ifelse(triggered, simd::float_4::mask(), attacking[c / 4]);

			// Get target and lambda for exponential decay
			const float attackTarget = 1.2f;
			simd::float_4 target = simd::ifelse(gate[c / 4], simd::ifelse(attacking[c / 4], attackTarget, sustainValueLambda[c / 4]), 0.f);
			simd::float_4 lambda = simd::ifelse(gate[c / 4], simd::ifelse(attacking[c / 4], attackValueLambda[c / 4], decayValueLambda[c / 4]), releaseValueLambda[c / 4]);

			// Adjust env
			env[c / 4] += (target - env[c / 4]) * lambda * args.sampleTime;

			// Turn off attacking state if envelope is HIGH
			attacking[c / 4] = simd::ifelse(env[c / 4] >= 1.f, simd::float_4::zero(), attacking[c / 4]);

			// Turn on attacking state if gate is LOW
			attacking[c / 4] = simd::ifelse(gate[c / 4], attacking[c / 4], simd::float_4::mask());

			// Set output
			outputs[ENVELOPEJACK_OUTPUT].setVoltageSimd(10.f * env[c / 4], c);
		}

		outputs[ENVELOPEJACK_OUTPUT].setChannels(channels);

		// Lights
		if (lightDivider.process()) {
			lights[STARTLED_LIGHT].setBrightness(0);
			lights[ATTACKLED_LIGHT].setBrightness(0);
			lights[HOLDLED_LIGHT].setBrightness(0);
			lights[DECAYLED_LIGHT].setBrightness(0);
			lights[SUSTAINLED_LIGHT].setBrightness(0);
			lights[DELAYLED_LIGHT].setBrightness(0);
			lights[RELEASELED_LIGHT].setBrightness(0);

			for (int channel = 0; channel < channels; channel += 4) {
				const float epsilon = 0.01f;
				//float_4 sustaining = (sustain[channel / 4] <= env[channel / 4]) & (env[channel / 4] < sustain[channel / 4] + epsilon);
				//float_4 resting = (env[channel / 4] < epsilon);

				simd::float_4 starting;
				simd::float_4 attacking;
				simd::float_4 holding;
				simd::float_4 decaing;
				simd::float_4 sustaining;
				simd::float_4 delaing;
				simd::float_4 releasing;

				if (simd::movemask(gate[channel / 4] & starting[channel / 4]))
					lights[STARTLED_LIGHT].setBrightness(1);
				if (simd::movemask(gate[channel / 4] & attacking[channel / 4]))
					lights[ATTACKLED_LIGHT].setBrightness(1);
				if (simd::movemask(gate[channel / 4] & holding[channel / 4]))
					lights[HOLDLED_LIGHT].setBrightness(1);
				if (simd::movemask(gate[channel / 4] & decaing[channel / 4]))
					lights[DECAYLED_LIGHT].setBrightness(1);
				if (simd::movemask(gate[channel / 4] & sustaining[channel / 4]))
					lights[SUSTAINLED_LIGHT].setBrightness(1);
				if (simd::movemask(gate[channel / 4] & delaing[channel / 4]))
					lights[DELAYLED_LIGHT].setBrightness(1);
				if (simd::movemask(~gate[channel / 4] & releasing))
					lights[RELEASELED_LIGHT].setBrightness(1);
			}
		}
	}
};


struct Envelope_1Widget : ModuleWidget {
	Envelope_1Widget(Envelope_1* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/envelope-1.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<AHMRoundKnobWhite>(mm2px(Vec(31.923, 14)), module, Envelope_1::STARTKNOB_PARAM));
		addParam(createParamCentered<AHMRoundKnobWhite>(mm2px(Vec(31.923, 28)), module, Envelope_1::ATTACKKNOB_PARAM));
		addParam(createParamCentered<AHMRoundKnobWhite>(mm2px(Vec(31.923, 42)), module, Envelope_1::HOLDKNOB_PARAM));
		addParam(createParamCentered<AHMRoundKnobWhite>(mm2px(Vec(31.923, 56)), module, Envelope_1::DECAYKNOB_PARAM));
		addParam(createParamCentered<AHMRoundKnobWhite>(mm2px(Vec(31.923, 70)), module, Envelope_1::SUSTAINKNOB_PARAM));
		addParam(createParamCentered<AHMRoundKnobWhite>(mm2px(Vec(31.923, 84)), module, Envelope_1::DELAYKNOB_PARAM));
		addParam(createParamCentered<AHMRoundKnobWhite>(mm2px(Vec(31.923, 98)), module, Envelope_1::RELEASEKNOB_PARAM));

		addParam(createParamCentered<AHMRoundKnobWhiteTiny>(mm2px(Vec(20.32, 28)), module, Envelope_1::ATTACKSLOPEKNOB_PARAM));
		addParam(createParamCentered<AHMRoundKnobWhiteTiny>(mm2px(Vec(20.32, 56)), module, Envelope_1::DECAYSLOPEKNOB_PARAM));
		addParam(createParamCentered<AHMRoundKnobWhiteTiny>(mm2px(Vec(20.32, 98)), module, Envelope_1::RELEASESLOPEKNOB_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.72, 14)), module, Envelope_1::STARTJACK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.72, 28)), module, Envelope_1::ATTACKJACK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.72, 42)), module, Envelope_1::HOLDJACK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.72, 56)), module, Envelope_1::DECAYJACK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.72, 70)), module, Envelope_1::SUSTAINJACK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.72, 84)), module, Envelope_1::DELAYJACK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.72, 98)), module, Envelope_1::RELEASEJACK_INPUT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.72, 112.69)), module, Envelope_1::GATEJACK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.32, 112.69)), module, Envelope_1::TRIGJACK_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(31.923, 112.69)), module, Envelope_1::ENVELOPEJACK_OUTPUT));

		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(36.92, 9)), module, Envelope_1::STARTLED_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(36.92, 23)), module, Envelope_1::ATTACKLED_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(36.92, 37)), module, Envelope_1::HOLDLED_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(36.92, 51)), module, Envelope_1::DECAYLED_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(36.92, 65)), module, Envelope_1::SUSTAINLED_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(36.92, 79)), module, Envelope_1::DELAYLED_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(36.92, 93)), module, Envelope_1::RELEASELED_LIGHT));
	}
};


Model* modelEnvelope_1 = createModel<Envelope_1, Envelope_1Widget>("Envelope-1");