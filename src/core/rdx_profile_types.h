#pragma once
// Capture coefficients after each envelope step, including stage transitions.
struct RdxEnvelopePoint {float level,k,c;bool rising;};
