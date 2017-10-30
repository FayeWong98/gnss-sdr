/*!
 * \file obs_gps_l1_space_system_test.cc
 * \brief  This class implements a test for the validation of generated observables.
 * \author Carles Fernandez-Prades, 2016. cfernandez(at)cttc.es
 *         Antonio Ramos, 2017. antonio.ramos(at)cttc.es
 *
 *
 * -------------------------------------------------------------------------
 *
 * Copyright (C) 2010-2017  (see AUTHORS file for a list of contributors)
 *
 * GNSS-SDR is a software defined Global Navigation
 *          Satellite Systems receiver
 *
 * This file is part of GNSS-SDR.
 *
 * GNSS-SDR is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GNSS-SDR is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNSS-SDR. If not, see <http://www.gnu.org/licenses/>.
 *
 * -------------------------------------------------------------------------
 */

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>
#include <unistd.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <gtest/gtest.h>
#include "RinexUtilities.hpp"
#include "Rinex3ObsBase.hpp"
#include "Rinex3ObsData.hpp"
#include "Rinex3ObsHeader.hpp"
#include "Rinex3ObsStream.hpp"
#include "concurrent_map.h"
#include "concurrent_queue.h"
#include "control_thread.h"
#include "file_configuration.h"
#include "signal_generator_flags.h"


// For GPS NAVIGATION (L1)
concurrent_queue<Gps_Acq_Assist> global_gps_acq_assist_queue;
concurrent_map<Gps_Acq_Assist> global_gps_acq_assist_map;

DEFINE_string(configuration_file_space, "./default_configuration.conf", "Path of configuration file");

class ObsGpsL1SpaceSystemTest: public ::testing::Test
{
public:
    //std::string generator_binary;
    //std::string p1;
    //std::string p2;
    //std::string p3;
    //std::string p4;
    //std::string p5;

    //const double baseband_sampling_freq = 2.6e6;

    std::string filename_rinex_obs = FLAGS_filename_rinex_obs;
    //std::string filename_raw_data = FLAGS_filename_raw_data;
    std::string generated_rinex_obs;
    std::string configuration_file_ = FLAGS_configuration_file_space;
    //int configure_generator();
    //int generate_signal();
    int configure_receiver();
    int run_receiver();
    void check_results();
    bool check_valid_rinex_nav(std::string filename);  // return true if the file is a valid Rinex navigation file.
    bool check_valid_rinex_obs(std::string filename);  // return true if the file is a valid Rinex observation file.
    double compute_stdev(const std::vector<double> & vec);

    std::shared_ptr<FileConfiguration> config;
};


bool ObsGpsL1SpaceSystemTest::check_valid_rinex_nav(std::string filename)
{
    bool res = false;
    res = gpstk::isRinexNavFile(filename);
    return res;
}


double ObsGpsL1SpaceSystemTest::compute_stdev(const std::vector<double> & vec)
{
    double sum__ = std::accumulate(vec.begin(), vec.end(), 0.0);
    double mean__ = sum__ / vec.size();
    double accum__ = 0.0;
    std::for_each (std::begin(vec), std::end(vec), [&](const double d) {
        accum__ += (d - mean__) * (d - mean__);
    });
    double stdev__ = std::sqrt(accum__ / (vec.size() - 1));
    return stdev__;
}


bool ObsGpsL1SpaceSystemTest::check_valid_rinex_obs(std::string filename)
{
    bool res = false;
    res = gpstk::isRinex3ObsFile(filename);
    return res;
}

/*
int ObsGpsL1SpaceSystemTest::configure_generator()
{
    // Configure signal generator
    generator_binary = FLAGS_generator_binary;

    p1 = std::string("-rinex_nav_file=") + FLAGS_rinex_nav_file;
    if(FLAGS_dynamic_position.empty())
        {
            p2 = std::string("-static_position=") + FLAGS_static_position + std::string(",") + std::to_string(std::min(FLAGS_duration * 10, 3000));
            if(FLAGS_duration > 300) std::cout << "WARNING: Duration has been set to its maximum value of 300 s" << std::endl;
        }
    else
        {
            p2 = std::string("-obs_pos_file=") + std::string(FLAGS_dynamic_position);
        }
    p3 = std::string("-rinex_obs_file=") + FLAGS_filename_rinex_obs; // RINEX 2.10 observation file output
    p4 = std::string("-sig_out_file=") + FLAGS_filename_raw_data; // Baseband signal output file. Will be stored in int8_t IQ multiplexed samples
    p5 = std::string("-sampling_freq=") + std::to_string(baseband_sampling_freq); //Baseband sampling frequency [MSps]
    return 0;
}
*/
/*
int ObsGpsL1SpaceSystemTest::generate_signal()
{
    pid_t wait_result;
    int child_status;

    char *const parmList[] = { &generator_binary[0], &generator_binary[0], &p1[0], &p2[0], &p3[0], &p4[0], &p5[0], NULL };

    int pid;
    if ((pid = fork()) == -1)
        perror("fork error");
    else if (pid == 0)
        {
            execv(&generator_binary[0], parmList);
            std::cout << "Return not expected. Must be an execv error." << std::endl;
            std::terminate();
        }

    wait_result = waitpid(pid, &child_status, 0);
    if (wait_result == -1) perror("waitpid error");
    EXPECT_EQ(true, check_valid_rinex_obs(filename_rinex_obs));
    std::cout << "Signal and Observables RINEX files created."  << std::endl;
    return 0;
}
*/

int ObsGpsL1SpaceSystemTest::configure_receiver()
{
    config = std::make_shared<FileConfiguration>(configuration_file_);
    return 0;
}


int ObsGpsL1SpaceSystemTest::run_receiver()
{
    std::shared_ptr<ControlThread> control_thread;
    control_thread = std::make_shared<ControlThread>(config);
    // start receiver
    try
    {
            control_thread->run();
    }
    catch(const boost::exception & e)
    {
            std::cout << "Boost exception: " << boost::diagnostic_information(e);
    }
    catch(const std::exception & ex)
    {
            std::cout  << "STD exception: " << ex.what();
    }
    // Get the name of the RINEX obs file generated by the receiver
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    FILE *fp;
    std::string argum2 = std::string("/bin/ls *O | grep GSDR | tail -1");
    char buffer[1035];
    fp = popen(&argum2[0], "r");
    if (fp == NULL)
        {
            std::cout << "Failed to run command: " << argum2 << std::endl;
            return -1;
        }
    while (fgets(buffer, sizeof(buffer), fp) != NULL)
        {
            std::string aux = std::string(buffer);
            ObsGpsL1SpaceSystemTest::generated_rinex_obs = aux.erase(aux.length() - 1, 1);
        }
    pclose(fp);
    return 0;
}


void ObsGpsL1SpaceSystemTest::check_results()
{
    std::vector<std::vector<std::pair<double, double>> > pseudorange_ref(33);
    std::vector<std::vector<std::pair<double, double>> > carrierphase_ref(33);
    std::vector<std::vector<std::pair<double, double>> > doppler_ref(33);

    std::vector<std::vector<std::pair<double, double>> > pseudorange_meas(33);
    std::vector<std::vector<std::pair<double, double>> > carrierphase_meas(33);
    std::vector<std::vector<std::pair<double, double>> > doppler_meas(33);

    // Open and read reference RINEX observables file
    try
    {
            gpstk::Rinex3ObsStream r_ref(FLAGS_filename_rinex_obs);
            r_ref.exceptions(std::ios::failbit);
            gpstk::Rinex3ObsData r_ref_data;
            gpstk::Rinex3ObsHeader r_ref_header;

            gpstk::RinexDatum dataobj;

            r_ref >> r_ref_header;

            while (r_ref >> r_ref_data)
                {
                    for (int myprn = 1; myprn < 33; myprn++)
                        {
                            gpstk::SatID prn( myprn, gpstk::SatID::systemGPS );
                            gpstk::CommonTime time = r_ref_data.time;
                            double sow(static_cast<gpstk::GPSWeekSecond>(time).sow);

                            gpstk::Rinex3ObsData::DataMap::iterator pointer = r_ref_data.obs.find(prn);
                            if( pointer ==  r_ref_data.obs.end() )
                                {
                                    // PRN not present; do nothing
                                }
                            else
                                {
                                    dataobj = r_ref_data.getObs(prn, "C1C",  r_ref_header);
                                    double P1 = dataobj.data;
                                    std::pair<double, double> pseudo(sow,P1);
                                    pseudorange_ref.at(myprn).push_back(pseudo);

                                    dataobj = r_ref_data.getObs(prn, "L1C",  r_ref_header);
                                    double L1 = dataobj.data;
                                    std::pair<double, double> carrier(sow, L1);
                                    carrierphase_ref.at(myprn).push_back(carrier);

                                    dataobj = r_ref_data.getObs(prn, "D1C",  r_ref_header);
                                    double D1 = dataobj.data;
                                    std::pair<double, double> doppler(sow, D1);
                                    doppler_ref.at(myprn).push_back(doppler);
                                }  // End of 'if( pointer == roe.obs.end() )'
                        } // end for
                } // end while
    } // End of 'try' block
    catch(const gpstk::FFStreamError& e)
    {
            std::cout << e;
            exit(1);
    }
    catch(const gpstk::Exception& e)
    {
            std::cout << e;
            exit(1);
    }
    catch (...)
    {
            std::cout << "unknown error.  I don't feel so well..." << std::endl;
            exit(1);
    }

    try
    {
            std::string arg2_gen = std::string("./") + ObsGpsL1SpaceSystemTest::generated_rinex_obs;
            gpstk::Rinex3ObsStream r_meas(arg2_gen);
            r_meas.exceptions(std::ios::failbit);
            gpstk::Rinex3ObsData r_meas_data;
            gpstk::Rinex3ObsHeader r_meas_header;
            gpstk::RinexDatum dataobj;

            r_meas >> r_meas_header;

            while (r_meas >> r_meas_data)
                {
                    for (int myprn = 1; myprn < 33; myprn++)
                        {
                            gpstk::SatID prn( myprn, gpstk::SatID::systemGPS );
                            gpstk::CommonTime time = r_meas_data.time;
                            double sow(static_cast<gpstk::GPSWeekSecond>(time).sow);

                            gpstk::Rinex3ObsData::DataMap::iterator pointer = r_meas_data.obs.find(prn);
                            if( pointer ==  r_meas_data.obs.end() )
                                {
                                    // PRN not present; do nothing
                                }
                            else
                                {
                                    dataobj = r_meas_data.getObs(prn, "C1C",  r_meas_header);
                                    double P1 = dataobj.data;
                                    std::pair<double, double> pseudo(sow, P1);
                                    pseudorange_meas.at(myprn).push_back(pseudo);

                                    dataobj = r_meas_data.getObs(prn, "L1C",  r_meas_header);
                                    double L1 = dataobj.data;
                                    std::pair<double, double> carrier(sow, L1);
                                    carrierphase_meas.at(myprn).push_back(carrier);

                                    dataobj = r_meas_data.getObs(prn, "D1C",  r_meas_header);
                                    double D1 = dataobj.data;
                                    std::pair<double, double> doppler(sow, D1);
                                    doppler_meas.at(myprn).push_back(doppler);
                                }  // End of 'if( pointer == roe.obs.end() )'
                        } // end for
                } // end while
    } // End of 'try' block
    catch(const gpstk::FFStreamError& e)
    {
            std::cout << e;
            exit(1);
    }
    catch(const gpstk::Exception& e)
    {
            std::cout << e;
            exit(1);
    }
    catch (...)
    {
            std::cout << "unknown error.  I don't feel so well..." << std::endl;
            exit(1);
    }

    // Time alignment
    std::vector<std::vector<std::pair<double, double>> > pseudorange_ref_aligned(33);
    std::vector<std::vector<std::pair<double, double>> > carrierphase_ref_aligned(33);
    std::vector<std::vector<std::pair<double, double>> > doppler_ref_aligned(33);

    std::vector<std::vector<std::pair<double, double>> >::iterator iter;
    std::vector<std::pair<double, double>>::iterator it;
    std::vector<std::pair<double, double>>::iterator it2;

    std::vector<std::vector<double>> pr_diff(33);
    std::vector<std::vector<double>> cp_diff(33);
    std::vector<std::vector<double>> doppler_diff(33);

    std::vector<std::vector<double>>::iterator iter_diff;
    std::vector<double>::iterator iter_v;

    int prn_id = 0;
    for(iter = pseudorange_ref.begin(); iter != pseudorange_ref.end(); iter++)
        {
            for(it = iter->begin(); it != iter->end(); it++)
                {
                    // If a measure exists for this sow, store it
                    for(it2 = pseudorange_meas.at(prn_id).begin(); it2 != pseudorange_meas.at(prn_id).end(); it2++)
                        {
                            if(std::abs(it->first - it2->first) < 0.01) // store measures closer than 10 ms.
                                {
                                    pseudorange_ref_aligned.at(prn_id).push_back(*it);
                                    pr_diff.at(prn_id).push_back(it->second - it2->second );
                                    //std::cout << "Sat " << prn_id << ": " << "PR_ref=" << it->second << "   PR_meas=" << it2->second << "    Diff:" << it->second - it2->second <<  std::endl;
                                }
                        }
                }
            prn_id++;
        }

    prn_id = 0;
    for(iter = carrierphase_ref.begin(); iter != carrierphase_ref.end(); iter++)
        {
            for(it = iter->begin(); it != iter->end(); it++)
                {
                    // If a measure exists for this sow, store it
                    for(it2 = carrierphase_meas.at(prn_id).begin(); it2 != carrierphase_meas.at(prn_id).end(); it2++)
                        {
                            if(std::abs(it->first - it2->first) < 0.01) // store measures closer than 10 ms.
                                {
                                    carrierphase_ref_aligned.at(prn_id).push_back(*it);
                                    cp_diff.at(prn_id).push_back(it->second - it2->second );
                                    // std::cout << "Sat " << prn_id << ": " << "Carrier_ref=" << it->second << "   Carrier_meas=" << it2->second << "    Diff:" << it->second - it2->second <<  std::endl;
                                }
                        }
                }
            prn_id++;
        }
    prn_id = 0;
    for(iter = doppler_ref.begin(); iter != doppler_ref.end(); iter++)
        {
            for(it = iter->begin(); it != iter->end(); it++)
                {
                    // If a measure exists for this sow, store it
                    for(it2 = doppler_meas.at(prn_id).begin(); it2 != doppler_meas.at(prn_id).end(); it2++)
                        {
                            if(std::abs(it->first - it2->first) < 0.01) // store measures closer than 10 ms.
                                {
                                    doppler_ref_aligned.at(prn_id).push_back(*it);
                                    doppler_diff.at(prn_id).push_back(it->second - it2->second );
                                }
                        }
                }
            prn_id++;
        }

    // Compute pseudorange error
    prn_id = 0;
    std::vector<double> mean_pr_diff_v;
    for(iter_diff = pr_diff.begin(); iter_diff != pr_diff.end(); iter_diff++)
        {
            // For each satellite with reference and measurements aligned in time
            int number_obs = 0;
            double mean_diff = 0.0;
            for(iter_v = iter_diff->begin(); iter_v != iter_diff->end(); iter_v++)
                {
                    mean_diff = mean_diff + *iter_v;
                    number_obs = number_obs + 1;
                }
            if(number_obs > 0)
                {
                    mean_diff = mean_diff / number_obs;
                    mean_pr_diff_v.push_back(mean_diff);
                    std::cout << "-- Mean pseudorange difference for sat " << prn_id << ": " << mean_diff;
                    double stdev_ = compute_stdev(*iter_diff);
                    std::cout << " +/- " << stdev_ ;
                    std::cout << " [m]" << std::endl;
                }
            else
                {
                    mean_diff = 0.0;
                }

            prn_id++;
        }
    double stdev_pr = compute_stdev(mean_pr_diff_v);
    std::cout << "Pseudorange diff error stdev = " << stdev_pr << " [m]" << std::endl;
    ASSERT_LT(stdev_pr, 10.0);

    // Compute carrier phase error
    prn_id = 0;
    std::vector<double> mean_cp_diff_v;
    for(iter_diff = cp_diff.begin(); iter_diff != cp_diff.end(); iter_diff++)
        {
            // For each satellite with reference and measurements aligned in time
            int number_obs = 0;
            double mean_diff = 0.0;
            for(iter_v = iter_diff->begin(); iter_v != iter_diff->end(); iter_v++)
                {
                    mean_diff = mean_diff + *iter_v;
                    number_obs = number_obs + 1;
                }
            if(number_obs > 0)
                {
                    mean_diff = mean_diff / number_obs;
                    mean_cp_diff_v.push_back(mean_diff);
                    std::cout << "-- Mean carrier phase difference for sat " << prn_id << ": " << mean_diff;
                    double stdev_pr_ = compute_stdev(*iter_diff);
                    std::cout << " +/- " << stdev_pr_ << " whole cycles (19 cm)" << std::endl;
                }
            else
                {
                    mean_diff = 0.0;
                }

            prn_id++;
        }

    // Compute Doppler error
    prn_id = 0;
    std::vector<double> mean_doppler_v;
    for(iter_diff = doppler_diff.begin(); iter_diff != doppler_diff.end(); iter_diff++)
        {
            // For each satellite with reference and measurements aligned in time
            int number_obs = 0;
            double mean_diff = 0.0;
            for(iter_v = iter_diff->begin(); iter_v != iter_diff->end(); iter_v++)
                {
                    //std::cout << *iter_v << std::endl;
                    mean_diff = mean_diff + *iter_v;
                    number_obs = number_obs + 1;
                }
            if(number_obs > 0)
                {
                    mean_diff = mean_diff / number_obs;
                    mean_doppler_v.push_back(mean_diff);
                    std::cout << "-- Mean Doppler difference for sat " << prn_id << ": " << mean_diff << " [Hz]" << std::endl;
                }
            else
                {
                    mean_diff = 0.0;
                }

            prn_id++;
        }

    double stdev_dp = compute_stdev(mean_doppler_v);
    std::cout << "Doppler error stdev = " << stdev_dp << " [Hz]" << std::endl;
    ASSERT_LT(stdev_dp, 10.0);
}


TEST_F(ObsGpsL1SpaceSystemTest, Observables_system_test)
{
    std::cout << "Validating input RINEX obs (TRUE) file: " << filename_rinex_obs << " ..." << std::endl;
    bool is_rinex_obs_valid = check_valid_rinex_obs(filename_rinex_obs);
    ASSERT_EQ(true, is_rinex_obs_valid) << "The RINEX observation file " << filename_rinex_obs << " is not well formed.";
    std::cout << "The file is valid." << std::endl;

    // Configure the signal generator
    //configure_generator();

    // Generate signal raw signal samples and observations RINEX file
    /*
    if(!FLAGS_disable_generator)
        {
            generate_signal();
        }

    std::cout << "Validating generated reference RINEX obs file: " << FLAGS_filename_rinex_obs << " ..." << std::endl;
    bool is_gen_rinex_obs_valid = check_valid_rinex_obs( "./" + FLAGS_filename_rinex_obs);
    EXPECT_EQ(true, is_gen_rinex_obs_valid) << "The RINEX observation file " << FLAGS_filename_rinex_obs << ", generated by gnss-sim, is not well formed.";
    std::cout << "The file is valid." << std::endl;
    */
    // Configure receiver
    configure_receiver();

    // Run the receiver
    ASSERT_EQ( run_receiver(), 0) << "Problem executing the software-defined signal generator";

    std::cout << "Validating RINEX obs file obtained by GNSS-SDR: " << ObsGpsL1SpaceSystemTest::generated_rinex_obs << " ..." << std::endl;
    bool is_gen_rinex_obs_valid = check_valid_rinex_obs( "./" + ObsGpsL1SpaceSystemTest::generated_rinex_obs);
    ASSERT_EQ(true, is_gen_rinex_obs_valid) << "The RINEX observation file " << ObsGpsL1SpaceSystemTest::generated_rinex_obs << ", generated by GNSS-SDR, is not well formed.";
    std::cout << "The file is valid." << std::endl;

    // Check results
    check_results();
}


int main(int argc, char **argv)
{
    std::cout << "Running GNSS-SDR in Space Observables validation test..." << std::endl;
    int res = 0;
    try
    {
            testing::InitGoogleTest(&argc, argv);
    }
    catch(...) {} // catch the "testing::internal::<unnamed>::ClassUniqueToAlwaysTrue" from gtest

    google::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);

    // Run the Tests
    try
    {
            res = RUN_ALL_TESTS();
    }
    catch(...)
    {
            LOG(WARNING) << "Unexpected catch";
    }
    google::ShutDownCommandLineFlags();
    return res;
}
