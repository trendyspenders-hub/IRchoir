# I.R.Choir — researched open-source building blocks
Reviewed 23 October 2026. These are candidates, not integrated dependencies
(except JUCE, already referenced by CMake). No AI conversion has been tested.

- RVC: https://github.com/RVC-Project/Retrieval-based-Voice-Conversion-WebUI
  Voice conversion training and inference. Main code MIT; dependency, pretrained
  content encoder, pitch extractor and downloaded voice model terms require
  separate checking. Candidate for offline conversion of a sung vocal into
  multiple licensed singer identities. Not a choir model by itself.
- Neutone SDK: https://github.com/Neutone/neutone_sdk
  LGPL-2.1. Wraps PyTorch audio models for testing in Neutone host plugins;
  handles buffering, sample rates and delay compensation. SDK availability
  does not confer rights to the proprietary I AM CHOIR models or host source.
- JUCE: https://github.com/juce-framework/JUCE
  AU/VST3 plugin and UI framework. AGPLv3/commercial dual licensing.
- ONNX Runtime: https://github.com/microsoft/onnxruntime
  MIT inference runtime. Candidate for embedding a compatible exported voice
  model in C++; model export and Mac performance must be validated.
- Rubber Band: https://github.com/breakfastquay/rubberband
  Pitch and timing processing for harmony and humanization. GPL-2.0-or-later
  or commercial licensing. Does not create new vocal identities.
- w-okada voice-changer: https://github.com/w-okada/voice-changer
  Reference application for real-time conversion. Current edition table lists
  RVC in its ONNX edition for M1 Mac. Review exact version and component/model
  terms; not a ready-to-rebrand Mac AU/VST3 plugin.

Recommended experiment: obtain consent-based singer recordings/models, render
several RVC singer variants offline, vary pitch/timing carefully, and evaluate
whether their combined result is convincing. Then benchmark an embedded engine
and integrate into JUCE. Neutone offers an alternative model-testing route.
A convincing ensemble is a hypothesis to validate by listening, not guaranteed
by selecting these libraries. The current source prototype remains DSP-only.
