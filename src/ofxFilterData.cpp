#include "ofxFilterData.h"

// --------------------------------------------------
void mat4rate::forward(glm::mat4 _m, RateForwardParams& p, int nElapsedFrames) {

	// Calculate the current euler angle (warping to correct dimension)
	glm::vec3 euler = glm::eulerAngles(getRotation(_m));
	if (size() > 0 && b[0]) {
		euler = getEulerWarped(euler, r[0]);
	}

    // Calculate the ease params
    glm::vec3 easeParams;
//    ofLogNotice("mat4rate") << "Ease Params";
    for (int i = 0; i < 3; i++) {
        
        // Option 1:
//        easeParams[i] = b[1] ? ofMap(glm::l2Norm((*this)[i][1]),
//                                     0.0,
//                                     p.maxSpeed[i]/ofxFilterUnits::one()->fps(),
//                                     ofxFilterUnits::one()->convertEaseParam(p.slowEaseParam, 60),
//                                     ofxFilterUnits::one()->convertEaseParam(p.fastEaseParam, 60),
//                                     true) :
//        ofxFilterUnits::one()->convertEaseParam(p.defaultEaseParam, 60);
        
        // Option 2:
        easeParams[i] = b[1] ? ofMap(glm::l2Norm((*this)[i][1]), 0.0, p.maxSpeed[i]/ofxFilterUnits::one()->fps(), p.slowEaseParam, p.fastEaseParam, true) : p.defaultEaseParam;
        easeParams[i] = ofxFilterUnits::one()->convertEaseParam(easeParams[i], 60);
    }
    
	// Update rates
	glm::vec3 _t = getTranslation(_m);
	glm::vec3 _r = euler;
	glm::vec3 _s = getScale(_m);
	glm::vec3 tmp;
	for (int i = 0; i < size(); i++) {

		// normalize changes that occur over multiple frames
		float div = (i == 1) ? float(nElapsedFrames) : 1.0;

		if (i == 0) {
			swap(_t, t[i]);
			swap(_r, r[i]);
			swap(_s, s[i]);
		}
		else {

			// TODO:
			// Should rates be eased incrementally, so higher order rates change slower? (mix now)
			// Or should rates change at the same time, so all rates change the same amount? 
			//	(mix applied after ALL updates)
            
			tmp = t[i];
			t[i] = glm::mix((t[i - 1] - _t) / div,
                            t[i],
                            pow(easeParams[0], pow(p.easeParamRatePower, i-1)));
			_t = tmp;

			tmp = r[i];
			r[i] = glm::mix((r[i - 1] - _r) / div,
                            r[i],
                            pow(easeParams[1], pow(p.easeParamRatePower, i-1)));
			_r = tmp;

			tmp = s[i];
			s[i] = glm::mix((s[i - 1] - _s) / div,
                            s[i],
                            pow(easeParams[2], pow(p.easeParamRatePower, i-1)));
			_s = tmp;
		}

		if (!b[i]) {
			b[i] = true;
			break;
		}
	}
}

// --------------------------------------------------
void mat4rate::applyFriction(RateFrictionParams& p) {

    // Don't apply friction if the rate power is negative
    if (p.ratePower < 0.0) return;
    
    // Get the fps-corrected friction
    float friction = ofxFilterUnits::one()->convertEaseParam(p.friction, 60);
    
	// Don't apply friction to lowest-order parameters (skip 0)
	for (int i = 1; i < size(); i++) {
		
		// Don't apply friction to parameters that don't exist yet
		if (!b[i]) break;
		
		for (int j = 0; j < 3; j++) {
            
            // The higher the rate order, the higher the friction
            (*this)[j][i] *= friction * pow(friction, p.ratePower * float(i-1));
		}
	}
}

// --------------------------------------------------
void mat4rate::backward(int nFrames) {
	
	// If there is no valid rate information, we cannot predict
	if (size() <= 1 || !b[1]) return;

	// Backpropogate the rates
	for (int f = 0; f < nFrames; f++) {
		for (int i = size() - 2; i >= 0; i--) {
			if (!b[i]) break;

			t[i] += t[i + 1];
			r[i] += r[i + 1];
			s[i] += s[i + 1];
		}
	}

	// TODO: Should rotations be normalized after back propogation?
}

// --------------------------------------------------
void mat4rate::reduceRates(RateReduceParams& p) {
    
    // Iterate through all 2+ order rates.
    // It doesn't matter the order in which we iterate, since
    // we are only comparing direction, and this function only changes
    // mangitude.
    for (int order = 2; order < size(); order++) {
        
        // If this rate is invalid, break because all rates to follow will also
        // be invalid.
        if (!b[order]) break;
        
        // Assume that if this is valid, lower order rates will also be valid.
        // For example, if acc is valid, vel should also be valid.
        
        // Iterate through t, r ,s
        for (int i = 0; i < 3; i++) {
            // How much will we reduce this rate?
            float mult = ofMap(glm::dot(glm::normalize((*this)[i][order-1]), glm::normalize((*this)[i][order])), -1, 1, 0, 1, true);
            if (isnan(mult)) continue;  // skip if invalid
            mult = pow(mult, p.power);
            mult = ofMap(mult, 0, 1, p.opposingDirMult, p.alignedDirMult, true);
            
            // Alt: Should reduction happen differently for the parallel and
            // perpendicular components of the rate (when projected onto the
            // next lowest order rate)?
            
            // Apply the rate
            (*this)[i][order] *= mult;
        }
    }
}

// --------------------------------------------------
bool ofxFilterData::converge(ofxFilterData& to, ConvergenceParams& p) {

    // Check to make sure rates are valid (and there are enough of them)
    if (r.size() < 3 || to.r.size() < 3 || !r.isOrderValid(2) || !to.r.isOrderValid(1)) return false;

    // Update the rates of every type of rate (translational, rotational, scalable):
    for (int i = 0; i < 3; i++) {

        // If FROM and TO frames are the same, then skip
        if (m == to.m) continue;
        
        // ------------- HEADING ---------------

        // First, find the vector to the target
        glm::vec3 heading;
        if (i == 1) { // rotation
            heading = getEulerWarped(to.r[i][0], r[i][0]) - r[i][0];
        }
        else {
            heading = to.r[i][0] - r[i][0];
        }

        // If the heading is too small, no changes need to be made to the rates.
        float epsilon = glm::l2Norm(heading);
        if (epsilon < pow(10, -p.epsilonPower)) continue;
//        ofLogNotice("FD") << "\t\tHeading\t\t\t" << heading;
        
        // ------------- TIME ---------------

        // Next, determine approximately how long it would take to approach the target.
        // k0 describes the dissimilarity of current and target velocities.
        float k0 = ofMap(glm::dot(glm::normalize(r[i][1]), glm::normalize(to.r[i][1])), -1, 1, 2, 1, true);
        if (isnan(k0)) k0 = 1.0;
        // k1 describes the dissimilarity of the current velocity and the heading
        float k1 = ofMap(glm::dot(glm::normalize(r[i][1]), glm::normalize(heading)), -1, 1, 2, 1, true);
        if (isnan(k1)) k1 = 1.0;
        // TimeToTarget is in seconds.
        float timeToTarget = (k0 * k1 * (glm::l2Norm(heading) / glm::l2Norm(r[i][1]))) / ofxFilterUnits::one()->fps();
//        ofLogNotice("FD") << "\t\tTime to Target\t" << timeToTarget;

        // ------------- VELOCITY MAGNITUDE (SPEED) ---------------
        
        // Determine the target speed.
        // First, what is the maximum allowable speed per frame?
        float maxSpeedPerFrame = p.maxSpeed[i] / ofxFilterUnits::one()->fps();
        // Calculate a parameter that defines how close we are to target in the range [0, 1],
        // where 0 = close and 1 = far.
        float paramToMaxSpeed = ofMap(timeToTarget / p.approachTime, p.approachBuffer, 1, 0, 1, true);
        // What was the speed in the last frame?
        float prevSpeed = glm::l2Norm(to.r[i][1]);
        // What is the ideal next speed?
        float idealSpeed = ofLerp(prevSpeed, maxSpeedPerFrame, paramToMaxSpeed);
        
        // Interpolate a speed, so speeds don't change too quickly (and cause motion
        // artifacts in higher-order rates).
        // This value should be normalized according to framerate, but in testing,
        // performance was poor with normalization on. Therefore, this parameter will
        // not be normalized.
        float targetSpeed = ofLerp(idealSpeed, prevSpeed, p.targetSpeedEaseParam);
        // If it were normalized, it would look like this:
//        float targetSpeed = ofLerp(idealSpeed, prevSpeed, ofxFilterUnits::one()->convertEaseParam(p.targetSpeedEaseParam, 60));

        // Alt: The target speed could be better eased with a limit to magnitude. (TODO)
//        ofLogNotice("FD") << "\t\tTarget Speed\t" << targetSpeed << "\tto: " << glm::l2Norm(to.r[i][1]) << "\t maxPerFrame: " << maxSpeedPerFrame << "\tparam: " << paramToMaxSpeed;

        // ------------- VELOCITY ---------------
        
        // Determine the target velocity.
        // This should be in the direction of the target with the target magnitude.
        glm::vec3 targetVel = glm::normalize(heading) * targetSpeed;
//        ofLogNotice("FD") << "\t\tTarget Vel\t\t" << targetVel;
        
        // ------------- ACCELERATION MAGNITUDE ---------------

        // Find the ideal acceleration. This is also the target direction.
        glm::vec3 idealAcc = targetVel - r[i][1];
        
        // Lerp the acceleration magnitude, so it doesn't change too quickly.
        // We must do this to round the corners when velocity changes.
        float accMagnitude = ofLerp(glm::l2Norm(idealAcc), glm::l2Norm(r[i][2]), ofxFilterUnits::one()->convertEaseParam(p.accMagEaseParam, 60));
        // Alt: lerp the lerp parameter based on acc magnitude.
//        float accMagnitude = ofLerp(glm::l2Norm(idealAcc), glm::l2Norm(r[i][2]), ofMap(glm::l2Norm(r[i][2]), 0, 0.1, p.accMagEaseParam, 0.5, true));
        // Alt: Limit the change of acc angle and acc magnitude each timelstep. (TODO)
        
        // ------------- TARGET ACCELERATION ---------------
        
        // Calculate the target acceleration.
        glm::vec3 targetAcc = (glm::l2Norm(idealAcc) == 0.0) ? glm::vec3(0,0,0) : (glm::normalize(idealAcc) * accMagnitude);
//        ofLogNotice("FD") << "\t\tTarget Acc\t\t" << targetAcc;

        // ------------- JERK ---------------
        
        // Calculate the required jerk (change in acceleration).
        // This is the target change in acc:
        glm::vec3 targetJerk = targetAcc - r[i][2];
        // The maximum allowable change in acc is:
        float maxAccStepPerFrame = p.maxSpeed[i] / pow(ofxFilterUnits::one()->fps(), p.accStepPower);
        // The magnitude of our change is:
        float jerkMagnitude = min(glm::l2Norm(targetJerk), maxAccStepPerFrame);
        // And the real change is: (check for a zero jerk to prevent NAN)
        glm::vec3 jerk = (glm::l2Norm(targetJerk) == 0.0) ? glm::vec3(0,0,0) : (glm::normalize(targetJerk) * jerkMagnitude);
//        ofLogNotice("FD") << "\t\tJerk is\t\t" << jerk;
        
        // ------------- ACCELERATION ---------------

        // Calculate the new (adjusted) accleration that would be required to
        // produce the motion we desire (convergence on the target frame) and set it.
        // The new acceleration is the previous plus the change (jerk).
        // This is the only parameter changed by this function.
        r[i][2] = r[i][2] + jerk;
    }

    return true;
}

// --------------------------------------------------
void ofxFilterData::updateRateFromFrame(int nElapsedFrames, mat4rate::RateForwardParams& p) {
    
	r.forward(m, p, nElapsedFrames);
}

// --------------------------------------------------
bool ofxFilterData::setFrameFromRate() {

	if (r.size() < 1 || !r.b[0]) return false;

	ofQuaternion q;
	q.set(0, 0, 0, 1);
	q.makeRotate(r.r[0].x, glm::vec3(1, 0, 0), r.r[0].z, glm::vec3(0, 0, 1), r.r[0].y, glm::vec3(0, 1, 0));
    
	set(r.t[0], quatConvert(q), r.s[0]);
	
	return true;
}


// --------------------------------------------------
void ofxFilterData::lerp(ofxFilterData& to, float amt) {

	glm::vec3 _t = translation() * (1.0 - amt) + to.translation() * amt;
	glm::quat _r = glm::slerp(rotation(), to.rotation(), amt);
	glm::vec3 _s = scale() * (1.0 - amt) + to.scale() * amt;
	
	set(_t, _r, _s);
}

// --------------------------------------------------
glm::vec3 ofxFilterData::translation() {
	return getTranslation(m);
}

// --------------------------------------------------
glm::quat ofxFilterData::rotation() {
	return getRotation(m);
}

// --------------------------------------------------
glm::vec3 ofxFilterData::scale() {
	return getScale(m);
}

// --------------------------------------------------
void ofxFilterData::reconcile(ofxFilterData& a, ReconciliationMode mode) {

	switch (mode) {
	case ofxFilterData::ReconciliationMode::OFXFILTERDATA_RECONCILE_COPY_FRAME: {

		// The frame is copied, along with the first rate (which is the also the
		// frame). This leaves the rates as-is. If the next velocity differs
		// significantly from the current one, then this may create high
		// accelerations (whiplash).

		bValid = a.bValid;
		m = a.m;
		if (r.size() > 0) {		// TODO (?): check for validity?
			r.b[0] = a.r.b[0];
			r.t[0] = a.r.t[0];
			r.r[0] = a.r.r[0];
			r.s[0] = a.r.s[0];
		}

	}; break;
	case ofxFilterData::ReconciliationMode::OFXFILTERDATA_RECONCILE_COPY_FRAME_AND_UPDATE_RATE: {

		// ===== UPDATE USING FRAME =====
		// Motion parameters are updated using this frame. This frame becomes
		// the new frame. If the observed and predicted higher order (2+) rates
		// differ significantly, there will be whiplash. 

		bValid = a.bValid;
		m = a.m;
        //        updateRateFromFrame(); // TODO: Uncomment
        ofLogError("ofxFilterData") << "This reconciliation mode has not been properly setup yet. Use a different one";

	}; break;
    case ofxFilterData::ReconciliationMode::OFXFILTERDATA_RECONCILE_COPY_FRAME_AND_VALID_RATES: {

        // ==== ALL (BUT ONLY VALID RATES) ====
        // Strange behaviors have been observed when, while linked and receiving invalid data, a single 
        // valid frame (with mostly invalid rates) is received, followed by more invalid data. 
        // If COPY_ALL were used, then these invalid rates would be copied to the convergence data 
        // and be persisted indefinitely. To counter this, it is recommended to copy everything (except those
        // rates which are invalid).

        bValid = a.bValid;
        m = a.m;
        r.copyValidFrom(a.r);

    }; break;
	case ofxFilterData::ReconciliationMode::OFXFILTERDATA_RECONCILE_COPY_ALL: default: {

		// ===== ALL =====
		// The frame and rate are copied from the observation
		// (This may result in uneven steps, since there is no reconciliation
		// between predictions and observations during this linked state. 
		// However, the rate information is reliable, since it comes 
		// directly from observations

		bValid = a.bValid;
		m = a.m;
		r = a.r;

	}; break;
	}
}

// --------------------------------------------------
bool ofxFilterData::similar(ofxFilterData& a, SimilarityParams& p) {

	// Are this data and the other data similar?

	float mix = 0;
	int nOrders = min(min(p.nRates, r.size()), a.r.size());
	for (int order = 0; order < nOrders; order++) {

		// Calculate the differences (l2 distances)
		glm::vec3 diff;
		if (order == 0) {
			// Look at the frame

			// Calculate the difference in translation.
			diff[0] = glm::l2Norm(translation(), a.translation());

			// Calculate the difference in rotation.
			// This should be done with angle() and axisAngle() of quats, but for
			// ease, we will use euler angles.
			// Warp the euler angles so they traverse the shortest path.
			glm::vec3 euler = glm::eulerAngles(rotation());
			glm::vec3 refEuler = glm::eulerAngles(a.rotation());
			euler = getEulerWarped(euler, refEuler);
			// Calc the difference
			diff[1] = glm::l2Norm(euler, refEuler);

			// Calculate the difference in scale.
			diff[2] = glm::l2Norm(scale(), a.scale());
		}
		else {
			// Look at the rates

			// Only proceed if valid
			if (!r.b[0] || !a.r.b[0]) continue;

			diff[0] = glm::l2Norm(r.t[order], a.r.t[order]);
			diff[1] = glm::l2Norm(r.r[order], a.r.r[order]);
			diff[2] = glm::l2Norm(r.s[order], a.r.s[order]);
		}

		// Apply the thresholds and sum them up
		for (int i = 0; i < 3; i++) {
            mix += diff[i] == 0.0 ? 0.0 : (log(diff[i] / (p.thresh[i] * pow(p.rateThreshMult, order))) * p.mix[i] * pow(2.0, -order * p.rateWeight));
		}
	}

	// The frames are similar if the mixture is less or equal to 0
	return mix <= 0.0;
}

// --------------------------------------------------
void ofxFilterData::clear() {

    bValid = true;
    m = glm::mat4();
    validMeasures = glm::bvec3(false, false, false);
    r.clear();
}

// --------------------------------------------------

// --------------------------------------------------

// --------------------------------------------------

// --------------------------------------------------

// --------------------------------------------------
