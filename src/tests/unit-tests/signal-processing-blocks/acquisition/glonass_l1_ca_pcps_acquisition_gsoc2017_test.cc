/*!
 * \file glonass_l1_ca_pcps_acquisition_gsoc2017_test.cc
 * \brief  Tests a PCPS acquisition block for Glonass L1 C/A signals
 * \author Gabriel Araujo, 2017. gabriel.araujo.5000(at)gmail.com
 * \author Luis Esteve, 2017. luis(at)epsilon-formacion.com
 *
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (C) 2010-2020  (see AUTHORS file for a list of contributors)
 *
 * GNSS-SDR is a software defined Global Navigation
 *          Satellite Systems receiver
 *
 * This file is part of GNSS-SDR.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * -----------------------------------------------------------------------------
 */


#include "concurrent_queue.h"
#include "configuration_interface.h"
#include "freq_xlating_fir_filter.h"
#include "gen_signal_source.h"
#include "glonass_l1_ca_pcps_acquisition.h"
#include "gnss_block_interface.h"
#include "gnss_sdr_valve.h"
#include "gnss_synchro.h"
#include "in_memory_configuration.h"
#include "pass_through.h"
#include "signal_generator.h"
#include "signal_generator_c.h"
#include <gnuradio/analog/sig_source_waveform.h>
#include <gnuradio/blocks/file_source.h>
#include <gnuradio/blocks/null_sink.h>
#include <gnuradio/top_block.h>
#include <gtest/gtest.h>
#include <pmt/pmt.h>
#include <chrono>
#include <memory>
#include <thread>
#include <utility>
#if HAS_GENERIC_LAMBDA
#else
#include <boost/bind/bind.hpp>
#endif
#ifdef GR_GREATER_38
#include <gnuradio/analog/sig_source.h>
#else
#include <gnuradio/analog/sig_source_c.h>
#endif

// ######## GNURADIO BLOCK MESSAGE RECEVER #########
class GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx;

#if GNURADIO_USES_STD_POINTERS
using GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx_sptr = std::shared_ptr<GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx>;
#else
using GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx_sptr = boost::shared_ptr<GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx>;
#endif

GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx_sptr GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx_make(Concurrent_Queue<int>& queue);


class GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx : public gr::block
{
private:
    friend GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx_sptr GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx_make(Concurrent_Queue<int>& queue);
    void msg_handler_channel_events(const pmt::pmt_t msg);
    explicit GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx(Concurrent_Queue<int>& queue);
    Concurrent_Queue<int>& channel_internal_queue;

public:
    int rx_message;
    ~GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx();  // Default destructor
};


GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx_sptr GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx_make(Concurrent_Queue<int>& queue)
{
    return GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx_sptr(new GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx(queue));
}


void GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx::msg_handler_channel_events(const pmt::pmt_t msg)
{
    try
        {
            int64_t message = pmt::to_long(std::move(msg));
            rx_message = message;
            channel_internal_queue.push(rx_message);
        }
    catch (const boost::bad_any_cast& e)
        {
            LOG(WARNING) << "msg_handler_channel_events Bad any_cast: " << e.what();
            rx_message = 0;
        }
}


GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx::GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx(Concurrent_Queue<int>& queue) : gr::block("GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx", gr::io_signature::make(0, 0, 0), gr::io_signature::make(0, 0, 0)), channel_internal_queue(queue)
{
    this->message_port_register_in(pmt::mp("events"));
    this->set_msg_handler(pmt::mp("events"),
#if HAS_GENERIC_LAMBDA
        [this](auto&& PH1) { msg_handler_channel_events(PH1); });
#else
#if USE_BOOST_BIND_PLACEHOLDERS
        boost::bind(&GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx::msg_handler_channel_events, this, boost::placeholders::_1));
#else
        boost::bind(&GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx::msg_handler_channel_events, this, _1));
#endif
#endif
    rx_message = 0;
}


GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx::~GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx() = default;


// ###########################################################

class GlonassL1CaPcpsAcquisitionGSoC2017Test : public ::testing::Test
{
protected:
    GlonassL1CaPcpsAcquisitionGSoC2017Test()
    {
        item_size = sizeof(gr_complex);
        stop = false;
        message = 0;
        gnss_synchro = Gnss_Synchro();
        acquisition = nullptr;
        init();
    }

    ~GlonassL1CaPcpsAcquisitionGSoC2017Test() = default;

    void init();
    void config_1();
    void config_2();
    void start_queue();
    void wait_message();
    void process_message();
    void stop_queue();

    Concurrent_Queue<int> channel_internal_queue;

    std::shared_ptr<Concurrent_Queue<pmt::pmt_t>> queue;
    gr::top_block_sptr top_block;
    std::shared_ptr<GlonassL1CaPcpsAcquisition> acquisition;
    std::shared_ptr<InMemoryConfiguration> config;
    Gnss_Synchro gnss_synchro;
    size_t item_size;
    bool stop;
    int message;
    std::thread ch_thread;

    unsigned int integration_time_ms = 0;
    unsigned int fs_in = 0;

    double expected_delay_chips = 0.0;
    double expected_doppler_hz = 0.0;
    float max_doppler_error_hz = 0.0;
    float max_delay_error_chips = 0.0;

    unsigned int num_of_realizations = 0;
    unsigned int realization_counter;
    unsigned int detection_counter;
    unsigned int correct_estimation_counter;
    unsigned int acquired_samples;
    unsigned int mean_acq_time_us;

    double mse_doppler;
    double mse_delay;

    double Pd;
    double Pfa_p;
    double Pfa_a;
};


void GlonassL1CaPcpsAcquisitionGSoC2017Test::init()
{
    message = 0;
    realization_counter = 0;
    detection_counter = 0;
    correct_estimation_counter = 0;
    acquired_samples = 0;
    mse_doppler = 0;
    mse_delay = 0;
    mean_acq_time_us = 0;
    Pd = 0;
    Pfa_p = 0;
    Pfa_a = 0;
}


void GlonassL1CaPcpsAcquisitionGSoC2017Test::config_1()
{
    gnss_synchro.Channel_ID = 0;
    gnss_synchro.System = 'R';
    std::string signal = "1G";
    signal.copy(gnss_synchro.Signal, 2, 0);

    integration_time_ms = 1;
    fs_in = 31.75e6;

    expected_delay_chips = 255;
    expected_doppler_hz = -1500;
    max_doppler_error_hz = 2 / (3 * integration_time_ms * 1e-3);
    max_delay_error_chips = 0.50;

    num_of_realizations = 1;

    config = std::make_shared<InMemoryConfiguration>();

    config->set_property("GNSS-SDR.internal_fs_sps", std::to_string(fs_in));

    config->set_property("SignalSource.fs_hz", std::to_string(fs_in));

    config->set_property("SignalSource.item_type", "gr_complex");

    config->set_property("SignalSource.num_satellites", "1");

    config->set_property("SignalSource.system_0", "R");
    config->set_property("SignalSource.PRN_0", "10");
    config->set_property("SignalSource.CN0_dB_0", "44");
    config->set_property("SignalSource.doppler_Hz_0", std::to_string(expected_doppler_hz));
    config->set_property("SignalSource.delay_chips_0", std::to_string(expected_delay_chips));

    config->set_property("SignalSource.noise_flag", "false");
    config->set_property("SignalSource.data_flag", "false");
    config->set_property("SignalSource.BW_BB", "0.97");

    config->set_property("InputFilter.implementation", "Freq_Xlating_Fir_Filter");
    config->set_property("InputFilter.input_item_type", "gr_complex");
    config->set_property("InputFilter.output_item_type", "gr_complex");
    config->set_property("InputFilter.taps_item_type", "float");
    config->set_property("InputFilter.number_of_taps", "11");
    config->set_property("InputFilter.number_of_bands", "2");
    config->set_property("InputFilter.band1_begin", "0.0");
    config->set_property("InputFilter.band1_end", "0.97");
    config->set_property("InputFilter.band2_begin", "0.98");
    config->set_property("InputFilter.band2_end", "1.0");
    config->set_property("InputFilter.ampl1_begin", "1.0");
    config->set_property("InputFilter.ampl1_end", "1.0");
    config->set_property("InputFilter.ampl2_begin", "0.0");
    config->set_property("InputFilter.ampl2_end", "0.0");
    config->set_property("InputFilter.band1_error", "1.0");
    config->set_property("InputFilter.band2_error", "1.0");
    config->set_property("InputFilter.filter_type", "bandpass");
    config->set_property("InputFilter.grid_density", "16");
    config->set_property("InputFilter.sampling_frequency", std::to_string(fs_in));
    config->set_property("InputFilter.IF", "4000000");

    config->set_property("Acquisition.item_type", "gr_complex");
    config->set_property("Acquisition.coherent_integration_time_ms",
        std::to_string(integration_time_ms));
    config->set_property("Acquisition.max_dwells", "1");
    config->set_property("Acquisition.implementation", "GLONASS_L1_CA_PCPS_Acquisition");
    // config->set_property("Acquisition.threshold", "2.5");
    config->set_property("Acquisition.pfa", "0.001");
    config->set_property("Acquisition.doppler_max", "10000");
    config->set_property("Acquisition.doppler_step", "250");
    config->set_property("Acquisition.bit_transition_flag", "false");
    config->set_property("Acquisition.dump", "false");
}


void GlonassL1CaPcpsAcquisitionGSoC2017Test::config_2()
{
    gnss_synchro.Channel_ID = 0;
    gnss_synchro.System = 'R';
    std::string signal = "1G";
    signal.copy(gnss_synchro.Signal, 2, 0);

    integration_time_ms = 1;
    fs_in = 31.75e6;

    expected_delay_chips = 374;
    expected_doppler_hz = -2000;
    max_doppler_error_hz = 2 / (3 * integration_time_ms * 1e-3);
    max_delay_error_chips = 0.50;

    num_of_realizations = 100;

    config = std::make_shared<InMemoryConfiguration>();

    config->set_property("GNSS-SDR.internal_fs_sps", std::to_string(fs_in));

    config->set_property("SignalSource.fs_hz", std::to_string(fs_in));

    config->set_property("SignalSource.item_type", "gr_complex");

    config->set_property("SignalSource.num_satellites", "4");

    config->set_property("SignalSource.system_0", "R");
    config->set_property("SignalSource.PRN_0", "10");
    config->set_property("SignalSource.CN0_dB_0", "44");
    config->set_property("SignalSource.doppler_Hz_0", std::to_string(expected_doppler_hz));
    config->set_property("SignalSource.delay_chips_0", std::to_string(expected_delay_chips));

    config->set_property("SignalSource.system_1", "R");
    config->set_property("SignalSource.PRN_1", "15");
    config->set_property("SignalSource.CN0_dB_1", "44");
    config->set_property("SignalSource.doppler_Hz_1", "1000");
    config->set_property("SignalSource.delay_chips_1", "100");

    config->set_property("SignalSource.system_2", "R");
    config->set_property("SignalSource.PRN_2", "21");
    config->set_property("SignalSource.CN0_dB_2", "44");
    config->set_property("SignalSource.doppler_Hz_2", "2000");
    config->set_property("SignalSource.delay_chips_2", "200");

    config->set_property("SignalSource.system_3", "R");
    config->set_property("SignalSource.PRN_3", "22");
    config->set_property("SignalSource.CN0_dB_3", "44");
    config->set_property("SignalSource.doppler_Hz_3", "3000");
    config->set_property("SignalSource.delay_chips_3", "300");

    config->set_property("SignalSource.noise_flag", "true");
    config->set_property("SignalSource.data_flag", "true");
    config->set_property("SignalSource.BW_BB", "0.97");

    config->set_property("InputFilter.implementation", "Freq_Xlating_Fir_Filter");
    config->set_property("InputFilter.input_item_type", "gr_complex");
    config->set_property("InputFilter.output_item_type", "gr_complex");
    config->set_property("InputFilter.taps_item_type", "float");
    config->set_property("InputFilter.number_of_taps", "11");
    config->set_property("InputFilter.number_of_bands", "2");
    config->set_property("InputFilter.band1_begin", "0.0");
    config->set_property("InputFilter.band1_end", "0.97");
    config->set_property("InputFilter.band2_begin", "0.98");
    config->set_property("InputFilter.band2_end", "1.0");
    config->set_property("InputFilter.ampl1_begin", "1.0");
    config->set_property("InputFilter.ampl1_end", "1.0");
    config->set_property("InputFilter.ampl2_begin", "0.0");
    config->set_property("InputFilter.ampl2_end", "0.0");
    config->set_property("InputFilter.band1_error", "1.0");
    config->set_property("InputFilter.band2_error", "1.0");
    config->set_property("InputFilter.filter_type", "bandpass");
    config->set_property("InputFilter.grid_density", "16");
    config->set_property("InputFilter.sampling_frequency", std::to_string(fs_in));
    config->set_property("InputFilter.IF", "4000000");

    config->set_property("Acquisition.item_type", "gr_complex");
    config->set_property("Acquisition.coherent_integration_time_ms",
        std::to_string(integration_time_ms));
    config->set_property("Acquisition.max_dwells", "1");
    config->set_property("Acquisition.implementation", "GLONASS_L1_CA_PCPS_Acquisition");
    config->set_property("Acquisition.pfa", "0.001");
    config->set_property("Acquisition.doppler_max", "10000");
    config->set_property("Acquisition.doppler_step", "250");
    config->set_property("Acquisition.bit_transition_flag", "false");
    config->set_property("Acquisition.dump", "false");
}


void GlonassL1CaPcpsAcquisitionGSoC2017Test::start_queue()
{
    stop = false;
    ch_thread = std::thread(&GlonassL1CaPcpsAcquisitionGSoC2017Test::wait_message, this);
}


void GlonassL1CaPcpsAcquisitionGSoC2017Test::wait_message()
{
    struct timeval tv;
    int64_t begin = 0;
    int64_t end = 0;

    while (!stop)
        {
            acquisition->reset();

            gettimeofday(&tv, nullptr);
            begin = tv.tv_sec * 1e6 + tv.tv_usec;

            channel_internal_queue.wait_and_pop(message);

            gettimeofday(&tv, nullptr);
            end = tv.tv_sec * 1e6 + tv.tv_usec;

            mean_acq_time_us += (end - begin);

            process_message();
        }
}


void GlonassL1CaPcpsAcquisitionGSoC2017Test::process_message()
{
    if (message == 1)
        {
            detection_counter++;

            // The term -5 is here to correct the additional delay introduced by the FIR filter
            // The value 511.0 must be a variable, chips/length
            double delay_error_chips = std::abs(static_cast<double>(expected_delay_chips) - (static_cast<double>(gnss_synchro.Acq_delay_samples) - 5.0) * 511.0 / (static_cast<double>(fs_in) * 1e-3));
            double doppler_error_hz = std::abs(expected_doppler_hz - gnss_synchro.Acq_doppler_hz);

            mse_delay += std::pow(delay_error_chips, 2);
            mse_doppler += std::pow(doppler_error_hz, 2);

            if ((delay_error_chips < max_delay_error_chips) && (doppler_error_hz < max_doppler_error_hz))
                {
                    correct_estimation_counter++;
                }
        }

    realization_counter++;

    std::cout << "Progress: " << round(static_cast<float>(realization_counter) / static_cast<float>(num_of_realizations) * 100.0) << "% \r" << std::flush;

    if (realization_counter == num_of_realizations)
        {
            mse_delay /= num_of_realizations;
            mse_doppler /= num_of_realizations;

            Pd = static_cast<double>(correct_estimation_counter) / static_cast<double>(num_of_realizations);
            Pfa_a = static_cast<double>(detection_counter) / static_cast<double>(num_of_realizations);
            Pfa_p = (static_cast<double>(detection_counter) - static_cast<double>(correct_estimation_counter)) / static_cast<double>(num_of_realizations);

            mean_acq_time_us /= num_of_realizations;

            stop_queue();
            top_block->stop();
        }
}


void GlonassL1CaPcpsAcquisitionGSoC2017Test::stop_queue()
{
    stop = true;
}


TEST_F(GlonassL1CaPcpsAcquisitionGSoC2017Test, Instantiate)
{
    config_1();
    acquisition = std::make_shared<GlonassL1CaPcpsAcquisition>(config.get(), "Acquisition", 1, 0);
}


TEST_F(GlonassL1CaPcpsAcquisitionGSoC2017Test, ConnectAndRun)
{
    int nsamples = floor(fs_in * integration_time_ms * 1e-3);
    std::chrono::time_point<std::chrono::system_clock> begin, end;
    std::chrono::duration<double> elapsed_seconds(0);
    queue = std::make_shared<Concurrent_Queue<pmt::pmt_t>>();
    top_block = gr::make_top_block("Acquisition test");

    config_1();
    acquisition = std::make_shared<GlonassL1CaPcpsAcquisition>(config.get(), "Acquisition", 1, 0);
    auto msg_rx = GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx_make(channel_internal_queue);

    ASSERT_NO_THROW({
        acquisition->connect(top_block);
        auto source = gr::analog::sig_source_c::make(fs_in, gr::analog::GR_SIN_WAVE, 1000, 1, gr_complex(0));
        auto valve = gnss_sdr_make_valve(sizeof(gr_complex), nsamples, queue.get());
        top_block->connect(source, 0, valve, 0);
        top_block->connect(valve, 0, acquisition->get_left_block(), 0);
        top_block->msg_connect(acquisition->get_right_block(), pmt::mp("events"), msg_rx, pmt::mp("events"));
    }) << "Failure connecting the blocks of acquisition test.";

    EXPECT_NO_THROW({
        begin = std::chrono::system_clock::now();
        top_block->run();  // Start threads and wait
        end = std::chrono::system_clock::now();
        elapsed_seconds = end - begin;
    }) << "Failure running the top_block.";

    std::cout << "Processed " << nsamples << " samples in " << elapsed_seconds.count() * 1e6 << " microseconds\n";
}


TEST_F(GlonassL1CaPcpsAcquisitionGSoC2017Test, ValidationOfResults)
{
    config_1();
    queue = std::make_shared<Concurrent_Queue<pmt::pmt_t>>();
    top_block = gr::make_top_block("Acquisition test");

    acquisition = acquisition = std::make_shared<GlonassL1CaPcpsAcquisition>(config.get(), "Acquisition", 1, 0);
    auto msg_rx = GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx_make(channel_internal_queue);

    ASSERT_NO_THROW({
        acquisition->set_channel(1);
    }) << "Failure setting channel.";

    ASSERT_NO_THROW({
        acquisition->set_gnss_synchro(&gnss_synchro);
    }) << "Failure setting gnss_synchro.";

    ASSERT_NO_THROW({
        acquisition->set_doppler_max(10000);
    }) << "Failure setting doppler_max.";

    ASSERT_NO_THROW({
        acquisition->set_doppler_step(500);
    }) << "Failure setting doppler_step.";

    ASSERT_NO_THROW({
        acquisition->connect(top_block);
        top_block->msg_connect(acquisition->get_right_block(), pmt::mp("events"), msg_rx, pmt::mp("events"));
    }) << "Failure connecting acquisition to the top_block.";

    acquisition->init();

    ASSERT_NO_THROW({
        std::shared_ptr<SignalGenerator> signal_generator = std::make_shared<SignalGenerator>(config.get(), "SignalSource", 0, 1, queue.get());
        std::shared_ptr<FreqXlatingFirFilter> filter = std::make_shared<FreqXlatingFirFilter>(config.get(), "InputFilter", 1, 1);
        signal_generator->connect(top_block);
        top_block->connect(signal_generator->get_right_block(), 0, filter->get_left_block(), 0);
        top_block->connect(filter->get_right_block(), 0, acquisition->get_left_block(), 0);
    }) << "Failure connecting the blocks of acquisition test.";

    // i = 0 --> satellite in acquisition is visible
    // i = 1 --> satellite in acquisition is not visible
    for (unsigned int i = 0; i < 2; i++)
        {
            init();

            if (i == 0)
                {
                    gnss_synchro.PRN = 10;  // This satellite is visible
                }
            else if (i == 1)
                {
                    gnss_synchro.PRN = 20;  // This satellite is not visible
                }

            acquisition->set_local_code();
            acquisition->set_state(1);  // Ensure that acquisition starts at the first sample
            start_queue();

            EXPECT_NO_THROW({
                top_block->run();  // Start threads and wait
            }) << "Failure running the top_block.";

            if (i == 0)
                {
                    EXPECT_EQ(1, message) << "Acquisition failure. Expected message: 1=ACQ SUCCESS.";
                    if (message == 1)
                        {
                            EXPECT_EQ(static_cast<unsigned int>(1), correct_estimation_counter) << "Acquisition failure. Incorrect parameters estimation.";
                        }
                }
            else if (i == 1)
                {
                    EXPECT_EQ(2, message) << "Acquisition failure. Expected message: 2=ACQ FAIL.";
                }

            ASSERT_NO_THROW({
                ch_thread.join();
            }) << "Failure while waiting the queue to stop";
        }
}


TEST_F(GlonassL1CaPcpsAcquisitionGSoC2017Test, ValidationOfResultsProbabilities)
{
    config_2();
    queue = std::make_shared<Concurrent_Queue<pmt::pmt_t>>();
    top_block = gr::make_top_block("Acquisition test");
    acquisition = std::make_shared<GlonassL1CaPcpsAcquisition>(config.get(), "Acquisition", 1, 0);
    auto msg_rx = GlonassL1CaPcpsAcquisitionGSoC2017Test_msg_rx_make(channel_internal_queue);

    ASSERT_NO_THROW({
        acquisition->set_channel(1);
    }) << "Failure setting channel.";

    ASSERT_NO_THROW({
        acquisition->set_gnss_synchro(&gnss_synchro);
    }) << "Failure setting gnss_synchro.";

    ASSERT_NO_THROW({
        acquisition->set_doppler_max(config->property("Acquisition.doppler_max", 10000));
    }) << "Failure setting doppler_max.";

    ASSERT_NO_THROW({
        acquisition->set_doppler_step(config->property("Acquisition.doppler_step", 500));
    }) << "Failure setting doppler_step.";

    ASSERT_NO_THROW({
        acquisition->connect(top_block);
        top_block->msg_connect(acquisition->get_right_block(), pmt::mp("events"), msg_rx, pmt::mp("events"));
    }) << "Failure connecting acquisition to the top_block.";

    acquisition->init();

    ASSERT_NO_THROW({
        std::shared_ptr<SignalGenerator> signal_generator = std::make_shared<SignalGenerator>(config.get(), "SignalSource", 0, 1, queue.get());
        std::shared_ptr<FreqXlatingFirFilter> filter = std::make_shared<FreqXlatingFirFilter>(config.get(), "InputFilter", 1, 1);
        signal_generator->connect(top_block);
        top_block->connect(signal_generator->get_right_block(), 0, filter->get_left_block(), 0);
        top_block->connect(filter->get_right_block(), 0, acquisition->get_left_block(), 0);
    }) << "Failure connecting the blocks of acquisition test.";

    std::cout << "Probability of false alarm (target) = " << 0.1 << '\n';

    // i = 0 --> satellite in acquisition is visible (prob of detection and prob of detection with wrong estimation)
    // i = 1 --> satellite in acquisition is not visible (prob of false detection)
    for (unsigned int i = 0; i < 2; i++)
        {
            init();

            if (i == 0)
                {
                    gnss_synchro.PRN = 10;  // This satellite is visible
                }
            else if (i == 1)
                {
                    gnss_synchro.PRN = 1;  // This satellite is not visible
                }

            acquisition->set_local_code();

            start_queue();

            EXPECT_NO_THROW({
                top_block->run();  // Start threads and wait
            }) << "Failure running the top_block."
               << '\n';

            if (i == 0)
                {
                    std::cout << "Estimated probability of detection = " << Pd << '\n';
                    std::cout << "Estimated probability of false alarm (satellite present) = " << Pfa_p << '\n';
                    std::cout << "Mean acq time = " << mean_acq_time_us << " microseconds.\n";
                }
            else if (i == 1)
                {
                    std::cout << "Estimated probability of false alarm (satellite absent) = " << Pfa_a << '\n';
                    std::cout << "Mean acq time = " << mean_acq_time_us << " microseconds.\n";
                }

            ASSERT_NO_THROW({
                ch_thread.join();
            }) << "Failure while waiting the queue to stop"
               << '\n';
        }
}
