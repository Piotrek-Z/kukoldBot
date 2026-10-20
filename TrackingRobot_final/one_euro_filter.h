#pragma once
#include <cmath>

class LowPassFilter {
public:
    LowPassFilter(double alpha = 0.5) : m_haty(0.0), m_initialized(false), m_alpha(alpha) {}

    double filter(double value, double alpha) {
        m_alpha = alpha;
        if (!m_initialized) {
            m_haty = value;
            m_initialized = true;
        } else {
            m_haty = m_alpha * value + (1.0 - m_alpha) * m_haty;
        }
        return m_haty;
    }

    void reset() { m_initialized = false; }

private:
    double m_haty;
    bool m_initialized;
    double m_alpha;
};

class OneEuroFilter {
public:
    OneEuroFilter(double minCutoff = 1.0, double beta = 0.007, double dCutoff = 1.0, double deadband = 0.8)
        : m_minCutoff(minCutoff), m_beta(beta), m_dCutoff(dCutoff), m_deadband(deadband),
        m_lastTime(-1.0), m_lastRaw(0.0), m_lastOutput(0.0) {}

    void setParameters(double minCutoff, double beta, double deadband = 0.8, double dCutoff = 1.0) {
        m_minCutoff = minCutoff;
        m_beta = beta;
        m_deadband = deadband;
        m_dCutoff = dCutoff;
    }

    double filter(double value, double timestampSec) {
        if (m_lastTime < 0.0) {
            m_lastTime = timestampSec;
            m_lastRaw = value;
            m_lastOutput = value;
            m_x.reset();
            m_dx.reset();
            return m_x.filter(value, 1.0);
        }

        // 1. Martwa strefa (Deadband): eliminacja mikrodrgań pikseli kamery
        if (std::abs(value - m_lastOutput) < m_deadband) {
            return m_lastOutput;
        }

        double dt = timestampSec - m_lastTime;
        m_lastTime = timestampSec;
        if (dt <= 1e-5) dt = 1e-5;

        // 2. Estymacja dynamiki zmiany (pochodna)
        double dx = (value - m_lastRaw) / dt;
        m_lastRaw = value;
        double edx = m_dx.filter(dx, alpha(dt, m_dCutoff));

        // 3. Dynamiczne pasmo odcięcia One-Euro:
        double cutoff = m_minCutoff + m_beta * std::abs(edx);
        m_lastOutput = m_x.filter(value, alpha(dt, cutoff));
        return m_lastOutput;
    }

    void reset() {
        m_lastTime = -1.0;
        m_lastRaw = 0.0;
        m_lastOutput = 0.0;
        m_x.reset();
        m_dx.reset();
    }

private:
    double alpha(double dt, double cutoff) {
        double tau = 1.0 / (2.0 * 3.14159265358979323846 * cutoff);
        return 1.0 / (1.0 + tau / dt);
    }

    double m_minCutoff;
    double m_beta;
    double m_dCutoff;
    double m_deadband;
    double m_lastTime;
    double m_lastRaw;
    double m_lastOutput;
    LowPassFilter m_x;
    LowPassFilter m_dx;
};