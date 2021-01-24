#pragma once
#include <Vst3/EffectModel.hpp>
#include <Process/Dataflow/TimeSignature.hpp>

#include <ossia/dataflow/fx_node.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/pod_vector.hpp>
#include <ossia/editor/scenario/time_signature.hpp>
#include <ossia/detail/logger.hpp>
#include <public.sdk/source/vst/hosting/eventlist.h>
#include <public.sdk/source/vst/hosting/parameterchanges.h>
namespace vst3
{

class param_queue
    : public Steinberg::Vst::IParamValueQueue
{
public:
  explicit param_queue(Steinberg::Vst::ParamID id): id{id} { }

  Steinberg::Vst::ParamID id{};
  ossia::small_vector<std::pair<int32_t, Steinberg::Vst::ParamValue>, 1> data;
  Steinberg::Vst::ParamValue lastValue{};

  Steinberg::tresult queryInterface(const Steinberg::TUID _iid, void** obj) override
  {
    return Steinberg::kResultOk;
  }
  Steinberg::uint32 addRef() override
  {
    return 1;
  }
  Steinberg::uint32 release() override
  {
    return 1;
  }

  Steinberg::Vst::ParamID getParameterId() override
  {
    return id;
  }
  Steinberg::int32 getPointCount() override
  {
    return data.size();
  }
  Steinberg::tresult getPoint(Steinberg::int32 index, Steinberg::int32& sampleOffset, Steinberg::Vst::ParamValue& value) override
  {
    if(index >= 0 && index < data.size())
      std::tie(sampleOffset, value) = data[index];
    else if(index == -1)
    {
      sampleOffset = 0;
      value = lastValue;
    }

    return Steinberg::kResultOk;
  }

  Steinberg::tresult addPoint(Steinberg::int32 sampleOffset, Steinberg::Vst::ParamValue value, Steinberg::int32& index) override
  {
    index = data.size();
    data.emplace_back(sampleOffset, value);
    return Steinberg::kResultOk;
  }
};

class param_changes
    : public Steinberg::Vst::IParameterChanges
{
public:
  std::vector<param_queue> queues;
  Steinberg::tresult queryInterface(const Steinberg::TUID _iid, void** obj) override
  {
    return Steinberg::kResultOk;
  }
  Steinberg::uint32 addRef() override
  {
    return 1;
  }
  Steinberg::uint32 release() override
  {
    return 1;
  }

  Steinberg::int32 getParameterCount () override
  {
    return queues.size();
  }

  param_queue* getParameterData (Steinberg::int32 index) override
  {
    return &queues[index];
  }

  param_queue* addParameterData (const Steinberg::Vst::ParamID& id, Steinberg::int32& index /*out*/) override
  {
    index = queues.size();
    queues.emplace_back(id);
    return &queues.back();
  }
};

class vst_node_base : public ossia::graph_node
{
public:
  Plugin fx{};
  // Each element is the amount of channels in a given in/out port
  ossia::small_pod_vector<int, 2> m_audioInputChannels{};
  ossia::small_pod_vector<int, 2> m_audioOutputChannels{};
  int m_totalAudioIns{};
  int m_totalAudioOuts{};
  int m_totalEventIns{};
  int m_totalEventOuts{};


protected:
  explicit vst_node_base(Plugin&& ptr) : fx{std::move(ptr)}
  {
    m_inlets.reserve(10);
    controls.reserve(10);

    struct vis {
      vst_node_base& self;
      void audioIn(const Steinberg::Vst::BusInfo& bus, int idx)
      {
        self.m_inlets.push_back(new ossia::audio_inlet);
        self.m_audioInputChannels.push_back(bus.channelCount);
        self.m_totalAudioIns += bus.channelCount;
      }
      void eventIn(const Steinberg::Vst::BusInfo& bus, int idx)
      {
        self.m_inlets.push_back(new ossia::midi_inlet);
        self.m_totalEventIns++;
      }
      void audioOut(const Steinberg::Vst::BusInfo& bus, int idx)
      {
        self.m_outlets.push_back(new ossia::audio_outlet);
        self.m_audioOutputChannels.push_back(bus.channelCount);
        self.m_totalAudioOuts += bus.channelCount;
      }
      void eventOut(const Steinberg::Vst::BusInfo& bus, int idx)
      {
        self.m_outlets.push_back(new ossia::midi_outlet);
        self.m_totalEventOuts++;
      }
    };

    forEachBus(vis{*this}, *fx.component);

    if(m_audioInputChannels.empty())
      m_audioInputChannels.push_back(2);
    if(m_audioOutputChannels.empty())
      m_audioOutputChannels.push_back(2);

    m_totalAudioIns = std::max(2, this->m_totalAudioIns);
    m_totalAudioOuts = std::max(2, this->m_totalAudioOuts);

    if (auto err = fx.processor->setProcessing(true);
        err != Steinberg::kResultOk && err != Steinberg::kNotImplemented) {
      ossia::logger().warn("Couldn't set VST3 processing: {}",  err);
    }
  }

  ~vst_node_base()
  {
    if (auto err = fx.processor->setProcessing(false);
        err != Steinberg::kResultOk && err != Steinberg::kNotImplemented) {
      ossia::logger().warn("Couldn't set VST3 processing: {}",  err);
    }
  }

  struct vst_control
  {
    Steinberg::Vst::ParamID idx{};
    std::size_t queue_idx{};
    ossia::value_port* port{};
  };

public:
  ossia::small_vector<vst_control, 16> controls;

  std::size_t add_control(ossia::value_inlet* inlet, Steinberg::Vst::ParamID id, float v)
  {
    auto queue_idx = this->m_inputChanges.queues.size();
    this->m_inputChanges.queues.emplace_back(id);
    this->m_inputChanges.queues.back().lastValue = v;

    controls.push_back({id, queue_idx, inlet->target<ossia::value_port>()});
    root_inputs().push_back(std::move(inlet));
    return queue_idx;
  }

  // Used when a control is changed from the ui.
  void set_control(std::size_t queue_idx, float value)
  {
    auto& queue = this->m_inputChanges.queues[queue_idx];
    queue.lastValue = value;
    queue.data.clear();
    queue.data.emplace_back(0, value);
  }

  void setControls()
  {
    for (vst_control& p : controls)
    {
      const auto& vec = p.port->get_data();
      if (vec.empty())
        continue;
      if (auto t = last(vec).target<float>())
      {
        double value = ossia::clamp<double>((double)*t, 0., 1.);
        auto& queue = m_inputChanges.queues[p.queue_idx];
        queue.data.clear();
        queue.data.emplace_back(0, value);
        queue.lastValue = value;
      }
    }
  }

  void dispatchMidi(ossia::midi_port& port)
  {
    // copy midi data
    auto& ip = port.messages;
    const auto n_mess = ip.size();
    if (n_mess == 0)
    {

      //f();
      return;
    }

    /*
    // -2 since two are already available ?
    const auto sz = sizeof(VstEvents) + sizeof(void*) * n_mess * 2;
    VstEvents* events = (VstEvents*)alloca(sz);
    std::memset(events, 0, sz);
    events->numEvents = n_mess;

    ossia::small_vector<VstMidiEvent, 16> vec;
    vec.resize(n_mess);
    std::size_t i = 0;
    for (rtmidi::message& mess : ip)
    {
      VstMidiEvent& e = vec[i];
      std::memset(&e, 0, sizeof(VstMidiEvent));

      e.type = kVstMidiType;
      e.byteSize = sizeof(VstMidiEvent);
      e.deltaFrames = mess.timestamp;
      e.flags = kVstMidiEventIsRealtime;

      std::memcpy(e.midiData, mess.bytes.data(), std::min(mess.bytes.size(), (std::size_t)4));
      // for (std::size_t k = 0, N = std::min(mess.bytes.size(),
      // (std::size_t)4); k < N; k++)
      //   e.midiData[k] = mess.bytes[k];

      events->events[i] = reinterpret_cast<VstEvent*>(&e);
      i++;
    }
    dispatch(effProcessEvents, 0, 0, events, 0.f);
    f();
    */
  }

  auto& preparePort(ossia::audio_port& port, int numChannels, std::size_t samples)
  {
    auto& ip = port.samples;
    ip.resize(numChannels);
    for (auto& i : ip)
      i.resize(samples);
    return ip;
  }

  void setupTimeInfo(const ossia::token_request& tk, ossia::exec_state_facade st)
  {
    using namespace Steinberg::Vst;
    using F = ProcessContext;
    Steinberg::Vst::ProcessContext& time_info = this->m_context;
    time_info.sampleRate = st.sampleRate();
    time_info.projectTimeSamples = tk.date.impl;

    time_info.systemTime = st.currentDate() - st.startDate();
    time_info.continousTimeSamples = time_info.projectTimeSamples; // TODO

    time_info.projectTimeMusic = tk.musical_start_position;
    time_info.barPositionMusic = tk.musical_start_last_bar;
    time_info.cycleStartMusic = 0.;
    time_info.cycleEndMusic = 0.;

    time_info.tempo = tk.tempo;
    time_info.timeSigNumerator = tk.signature.upper;
    time_info.timeSigDenominator = tk.signature.lower;

    // time_info.chord = ....;

    time_info.smpteOffsetSubframes = 0;
    time_info.frameRate = {};
    time_info.samplesToNextClock = 0;
    time_info.state =
        F::kPlaying |
        F::kSystemTimeValid | F::kContTimeValid |
        F::kProjectTimeMusicValid | F::kBarPositionValid |
        F::kTempoValid | F::kTimeSigValid
    ;
  }


  Steinberg::Vst::ProcessData m_vstData;
  ossia::small_vector<Steinberg::Vst::AudioBusBuffers, 1> m_vstInput;
  ossia::small_vector<Steinberg::Vst::AudioBusBuffers, 1> m_vstOutput;

  Steinberg::Vst::ProcessContext m_context;
  param_changes m_inputChanges;
  param_changes m_outputChanges;
  Steinberg::Vst::EventList m_inputEvents;
  Steinberg::Vst::EventList m_outputEvents;
};

template <bool UseDouble>
class vst_node final : public vst_node_base
{
public:
  vst_node(Plugin dat, int sampleRate) : vst_node_base{std::move(dat)}
  {
    m_vstData.processMode = Steinberg::Vst::ProcessModes::kRealtime;
    if constexpr(UseDouble)
      m_vstData.symbolicSampleSize = Steinberg::Vst::kSample64;
    else
      m_vstData.symbolicSampleSize = Steinberg::Vst::kSample32;

    m_vstData.numInputs = m_audioInputChannels.size();
    m_vstData.numOutputs = m_audioOutputChannels.size();
    m_vstInput.resize(m_audioInputChannels.size());
    m_vstOutput.resize(m_audioOutputChannels.size());
    for(int i = 0; i < m_audioInputChannels.size(); i++)
    {
      m_vstInput[i].numChannels = m_audioInputChannels[i];
    }
    for(int i = 0; i < m_audioOutputChannels.size(); i++)
    {
      m_vstOutput[i].numChannels = m_audioOutputChannels[i];
    }

    m_vstData.inputs = m_vstInput.data();
    m_vstData.outputs = m_vstOutput.data();
    m_vstData.inputParameterChanges = &m_inputChanges;
    m_vstData.outputParameterChanges = &m_outputChanges;
    m_vstData.inputEvents = &m_inputEvents;
    m_vstData.outputEvents = &m_outputEvents;
    m_vstData.processContext = &m_context;

    /*

    dispatch(effSetSampleRate, 0, sampleRate, nullptr, sampleRate);
    dispatch(effSetBlockSize, 0, 4096, nullptr, 4096); // Generalize what's in pd
    dispatch(
        effSetProcessPrecision, 0, UseDouble ? kVstProcessPrecision64 : kVstProcessPrecision32);
    dispatch(effMainsChanged, 0, 1);
    dispatch(effStartProcess);

    fx->fx->resvd2 = reinterpret_cast<intptr_t>(this);
    */
  }

  ~vst_node()
  {
    /*
    fx->fx->resvd2 = 0;
    dispatch(effStopProcess);
    dispatch(effMainsChanged, 0, 0);
    */
  }

  std::string label() const noexcept override { return "VST3"; }

  void all_notes_off() noexcept override
  {
    /*
    if constexpr (IsSynth)
    {
      // copy midi data
      // should be 16 but some VSTs read a bit out of bounds apparently !
      constexpr auto sz = sizeof(VstEvents) + sizeof(void*) * 16 * 2;
      VstEvents* events = (VstEvents*)alloca(sz);
      events->numEvents = 16;

      VstMidiEvent ev[16] = {};
      std::memset(events, 0, sz);

      for (int i = 0; i < 16; i++)
      {
        auto& e = ev[i];
        e.type = kVstMidiType;
        e.byteSize = sizeof(VstMidiEvent);

        e.midiData[0] = (char)(uint8_t)176;
        e.midiData[1] = (char)(uint8_t)123;
        e.midiData[2] = 0;
        e.midiData[3] = 0;

        events->events[i] = reinterpret_cast<VstEvent*>(&e);
      }

      dispatch(effProcessEvents, 0, 0, events, 0.f);

      if constexpr(!UseDouble)
      {
        constexpr int samples = 64;
        float dummy[samples] = { 0.f };

        float** output = (float**)alloca(sizeof(float*) * std::max(2, this->fx->fx->numOutputs));
        for (int i = 0; i < this->fx->fx->numOutputs; i++)
          output[i] = dummy;

        fx->fx->processReplacing(fx->fx, output, output, samples);
      }
      else
      {
        constexpr int samples = 64;
        double dummy[samples] = { 0.f };

        double** output = (double**)alloca(sizeof(double*) * std::max(2, this->fx->fx->numOutputs));
        for (int i = 0; i < this->fx->fx->numOutputs; i++)
          output[i] = dummy;

        fx->fx->processDoubleReplacing(fx->fx, output, output, samples);

      }
    }
    */
  }

  void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
  {
    if (!muted() && tk.date > tk.prev_date)
    {
      const std::size_t samples = tk.physical_write_duration(st.modelToSamples());
      this->setControls();
      this->setupTimeInfo(tk, st);

      for(int i = m_totalAudioIns; i < m_totalAudioIns + m_totalEventIns; i++)
        dispatchMidi(*m_inlets[i]->template target<ossia::midi_port>());

      if constexpr (UseDouble)
      {
        processDouble(samples);
      }
      else
      {
        processFloat(samples);
      }
    }
  }

  void processFloat(std::size_t samples)
  {
    // In the float case we have temporary buffers for conversion
    if constexpr (!UseDouble)
    {
      // copy audio data
      float** input = (float**)alloca(sizeof(float*) * m_totalAudioIns);
      float** output = (float**)alloca(sizeof(float*) * m_totalAudioOuts);

      float_v.resize(std::max(m_totalAudioIns, m_totalAudioOuts));
      for(auto& v : float_v)
        v.resize(samples);

      int channel_k = 0;
      int float_k = 0;
      for(int i = 0; i < m_audioInputChannels.size(); i++)
      {
        const int numChannels = m_audioInputChannels[i];
        auto& port = *m_inlets[i]->template target<ossia::audio_port>();
        auto& ip = preparePort(port, numChannels, samples);

        Steinberg::Vst::AudioBusBuffers& vst_in = m_vstInput[i];
        vst_in.channelBuffers32 = input + channel_k;
        vst_in.silenceFlags = 0;

        for(int k = 0; k < numChannels; k++)
        {
          std::copy_n(ip[k].data(), std::min(samples, ip[k].size()), float_v[float_k].data());
          input[channel_k] = float_v[float_k].data();
          channel_k++;
          float_k++;
        }
      }

      channel_k = 0;
      float_k = 0;
      for(int i = 0; i < m_audioOutputChannels.size(); i++)
      {
        const int numChannels = m_audioOutputChannels[i];
        auto& port = *m_outlets[i]->template target<ossia::audio_port>();
        auto& op = preparePort(port, numChannels, samples);

        Steinberg::Vst::AudioBusBuffers& vst_out = m_vstOutput[i];
        vst_out.channelBuffers32 = output + channel_k;
        vst_out.silenceFlags = 0;
        for(int k = 0; k < numChannels; k++)
        {
          output[channel_k] = float_v[float_k].data();
          channel_k++;
          float_k++;
        }
      }

      {
        m_vstData.numSamples = samples;

        fx.processor->process(m_vstData);
      }

      channel_k = 0;
      float_k = 0;
      for(int i = 0; i < m_audioOutputChannels.size(); i++)
      {
        const int numChannels = m_audioOutputChannels[i];
        ossia::audio_port& port = *m_outlets[i]->template target<ossia::audio_port>();
        for(int k = 0; k < numChannels; k++)
        {
          auto& audio_out = port.samples[k];
          std::copy_n(float_v[float_k].data(), samples, audio_out.data());
        }
      }
    }
  }

  void processDouble(std::size_t samples)
  {
    // In the double case we use directly the buffers that are part of the
    // input & output ports
    if constexpr (UseDouble)
    {
      // copy audio data
      double** input = (double**)alloca(sizeof(double*) * m_totalAudioIns);
      double** output = (double**)alloca(sizeof(double*) * m_totalAudioOuts);

      int channel_k = 0;
      for(int i = 0; i < m_audioInputChannels.size(); i++)
      {
        const int numChannels = m_audioInputChannels[i];
        auto& port = *m_inlets[i]->template target<ossia::audio_port>();
        auto& ip = preparePort(port, numChannels, samples);

        Steinberg::Vst::AudioBusBuffers& vst_in = m_vstInput[i];
        vst_in.channelBuffers64 = input + channel_k;
        vst_in.silenceFlags = 0;
        for(int k = 0; k < numChannels; k++)
        {
          input[channel_k++] = ip[k].data();
        }
      }

      channel_k = 0;
      for(int i = 0; i < m_audioOutputChannels.size(); i++)
      {
        const int numChannels = m_audioOutputChannels[i];
        auto& port = *m_outlets[i]->template target<ossia::audio_port>();
        auto& op = preparePort(port, numChannels, samples);

        Steinberg::Vst::AudioBusBuffers& vst_out = m_vstOutput[i];
        vst_out.channelBuffers64 = output + channel_k;
        vst_out.silenceFlags = 0;
        for(int k = 0; k < numChannels; k++)
        {
          output[channel_k++] = op[k].data();
        }
      }

      {
        m_vstData.numSamples = samples;

        fx.processor->process(m_vstData);
      }
    }
  }

  struct dummy_t { };
  std::conditional_t<!UseDouble, std::vector<ossia::float_vector>, dummy_t> float_v;
};

template <bool b1, typename... Args>
auto make_vst_fx(Args&... args)
{
  return std::make_shared<vst_node<b1>>(args...);
}
}