//******************************************************************************
//* File:   ArucoBoard.cpp
//* Author: Jon Newman <jpnewman snail mit dot edu>
//*
//* Copyright (c) Jon Newman (jpnewman snail mit dot edu)
//* All right reserved.
//* This file is part of the Oat project.
//* This is free software: you can redistribute it and/or modify
//* it under the terms of the GNU General Public License as published by
//* the Free Software Foundation, either version 3 of the License, or
//* (at your option) any later version.
//* This software is distributed in the hope that it will be useful,
//* but WITHOUT ANY WARRANTY; without even the implied warranty of
//* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//* GNU General Public License for more details.
//* You should have received a copy of the GNU General Public License
//* along with this source code.  If not, see <http://www.gnu.org/licenses/>.
//****************************************************************************

#include "ArucoBoard.h"

#include <cmath>
#include <cpptoml.h>
#include <opencv2/aruco.hpp>
#include <opencv2/cvconfig.h>
#include <opencv2/opencv.hpp>
#include <string>

#include "../../lib/utility/IOFormat.h"
#include "../../lib/utility/TOMLSanitize.h"
#include "../../lib/utility/make_unique.h"

namespace oat {

po::options_description ArucoBoard::options() const
{
    po::options_description local_opts;
    local_opts.add_options()
        ("dictionary,D", po::value<std::string>(),
         "Aruco board dictionary to use for detection or printing when -p is "
         "defined. Dictionaries are defined by the size of each marker and the "
         "number of markers in the dictionary. These parameters are encoded by "
         "a string of the form:\n\n"
         "  <Size>X<Size>_<Number of Markers>\n\n"
         "Values:\n"
         "  4X4_50 (default)\n"
         "  4X4_100\n"
         "  4X4_250\n"
         "  4X4_1000\n"
         "  5X5_50\n"
         "  5X5_100\n"
         "  5X5_250\n"
         "  5X5_1000\n"
         "  6X6_50\n"
         "  6X6_100\n"
         "  6X6_250\n"
         "  6X6_1000\n"
         "  7X7_50\n"
         "  7X7_100\n"
         "  7X7_250\n"
         "  7X7_1000\n")
        ("camera-matrix,k", po::value<std::string>(),
         "Nine element float array, [K11,K12,...,K33], specifying the 3x3 "
         "camera matrix for your imaging setup. Generated by oat-calibrate.")
        ("distortion-coeffs,d", po::value<std::string>(),
         "Five to eight element float array, [x1,x2,x3,...], specifying lens "
         "distortion coefficients. Generated by oat-calibrate.")
        ("board-size,S", po::value<std::string>(),
         "Two element int array, [X,Y], specifying the dimensions of the Aruco "
         "board (the number of markers in the X and Y directions).")
        ("length,l", po::value<float>(),
         "Length, in meters, of each side of the square markers within the "
         "Aruco board.")
        ("separation,s", po::value<float>(),
         "Separation, in meters, between each of the markers within the "
         "Aruco board.")
        ("refine-detection,R",
         "Perform a secondary marker location refinement step using knowledge of "
         "the board layout after initial marker detection is performed. Can lead "
         "to improved pose estimation robustness.")
        ("print,p",
         "Prior to performing position detection, print the specified Aruco "
         "marker to a PNG file, named \'board.png\', in the current directory.")
        ("print-scale,P", po::value<int>(),
         "The number of pixels to map to marker length to determine printing "
         "resolution. For instance, print-scale 50 indicates that each side of "
         "the marker will be 50 pixels. Defaults to 100.")
        ("thresh-params,t", po::value<std::string>(),
         "Three element vector, [min,max,step], specifying threshold "
         "parameters for marker candidate detection. Min and max represent the "
         "interval where the thresholding window sizes (in pixels) are selected "
         "for adaptive thresholding. Step determines the granularity of "
         "increments between min and max. See cv::threshold() for details. "
         "Defaults to [3, 23, 10]")
        ("contour-params,t", po::value<std::string>(),
         "Two element vector, [min,max], specifying the minimum and maximum "
         "perimeter distance relative to the major dimension of the input frame "
         "in order for a detected contour to be considered a marker candidate. "
         "Defaults to [0.03, 4.0]. Note that a max=4.0 indicates that the marker "
         "can fill the entire frame.")
        ("min-corner-dist,o", po::value<double>(),
         "Float specifying the minimum distance between the corners of the same "
         "marker (expressed as rate relative the marker perimeter. Defaults to "
         "0.05.")
        ("min-marker-dist,O", po::value<double>(),
         "Float specifying the minimum distance between the corners of different "
         "markers (expressed as rate relative the minimum candidate marker "
         "perimeter. Defaults to 0.05.")
        ("min-border-dist,b", po::value<int>(),
         "Float specifying the minimum abosolute distance between a marker "
         "corner and the frame border (pixels). Defaults to 3.")
        ("pixels-per-cell,x", po::value<int>(),
         "Int specifying the number of pixels (length of a side) used to "
         "represent each black or white cell of the detected markers. A higher "
         "value may improve decoding accuracy at the cost of perforamce. "
         "Defaults to 4.")
        ("border-error-rate,B", po::value<double>(),
         "Fraction of board bits that can be white (erroneous) instead of black. "
         "Defaults to 0.35")
        ("tune,t",
         "If true, provide a GUI with sliders for tuning detection "
         "parameters.")
        ;

    return local_opts;
}

void ArucoBoard::applyConfiguration(const po::variables_map &vm,
                                    const config::OptionTable &config_table)
{
    // Marker dictionary
    std::string dict_key = "4X4_50";
    oat::config::getValue<std::string>(
        vm, config_table, "dictionary", dict_key);
    auto dict = cv::aruco::getPredefinedDictionary(arucoDictionaryID(dict_key));

    // Board dimensions
    std::vector<int> n;
    if (oat::config::getArray<int, 2>(vm, config_table, "board-size", n, true)) {

        if (n[0] < 1 || n[1] < 1) {
            throw std::runtime_error(
                "Board size values must be greater than one.");
        }

        if (n[0] * n[1] > dict->bytesList.rows) { // bytesList.rows is dict size
            throw std::runtime_error(
                "Board size is too large for selected dictionary.");
        }
    }

    // Length
    oat::config::getNumericValue<float>(vm,
                                        config_table,
                                        "length",
                                        marker_length_,
                                        0,
                                        std::numeric_limits<float>::max(),
                                        true);

    // Separation
    float marker_separation;
    oat::config::getNumericValue<float>(vm,
                                        config_table,
                                        "separation",
                                        marker_separation,
                                        0,
                                        std::numeric_limits<float>::max(),
                                        true);

    // Detection parameters struct
    dp_ = cv::aruco::DetectorParameters::create();

    // Threshold params
    std::vector<int> p;
    if (oat::config::getArray<int, 3>(vm, config_table, "thresh-params", p)) {

        if (p[0] < 3 || p[1] < 1 || p[2] < 1) {
            throw std::runtime_error(
                "Threshold parameters must be: min >=3, max > min, step >= 1.");
        }

        dp_->adaptiveThreshWinSizeMin = p[0];
        dp_->adaptiveThreshWinSizeMax = p[1];
        dp_->adaptiveThreshWinSizeStep = p[2];
    }

    // Contour params
    std::vector<double> c;
    if (oat::config::getArray<double, 2>(vm, config_table, "contour-params", c)) {

        if (c[0] < 0 || c[1] < 0 ) {
            throw std::runtime_error(
                "Contour parameters must be positive numbers.");
        }

        dp_->minMarkerPerimeterRate = c[0];
        dp_->maxMarkerPerimeterRate = c[1];
    }

    // Min corner distance
    oat::config::getNumericValue<double>(
        vm, config_table, "min-corner-dist", dp_->minCornerDistanceRate, 0);

    // Min marker distance
    oat::config::getNumericValue<double>(
        vm, config_table, "min-marker-dist", dp_->minMarkerDistanceRate, 0);

    // Min border distance
    oat::config::getNumericValue<int>(
        vm, config_table, "min-border-dist", dp_->minDistanceToBorder, 0);

    // Pixels per cell
    oat::config::getNumericValue<int>(
        vm, config_table, "pixels-per-cell", dp_->perspectiveRemovePixelPerCell, 0);

    // Border error rate
    oat::config::getNumericValue<double>(
        vm, config_table, "border-error-rate", dp_->maxErroneousBitsInBorderRate, 0);

    // Create the board
    pGB gb = cv::aruco::GridBoard::create(
        n[0], n[1], marker_length_, marker_separation, dict);
    board_ = gb.staticCast<cv::aruco::Board>();

    // Refine detection flag
    oat::config::getValue<bool>(
        vm, config_table, "refine-detection", refine_detection_);

    // Print scale
    int scale = 100;
    oat::config::getNumericValue<int>(
        vm, config_table, "print-scale", scale, 0);

    // Print board to file
    bool print_board = false;
    oat::config::getValue<bool>(vm, config_table, "print", print_board);

    if (print_board) {
        cv::Mat board_img;
        gb->draw(cv::Size(n[0] * scale, n[1] * scale), board_img);
        cv::imwrite("board.png", board_img);
    }

    // Distortion coefficients
    if (oat::config::getArray<double>(
            vm, config_table, "distortion-coeffs", dist_coeff_, true)) {

        if (dist_coeff_.size() < 5 || dist_coeff_.size() > 8) {
            throw(std::runtime_error(
                "Distortion coefficients consist of 5 to 8 values."));
        }
    }

    // Camera Matrix
    std::vector<double> K;
    if (oat::config::getArray<double, 9>(vm, config_table, "camera-matrix", K, true)) {

        camera_matrix_(0, 0) = K[0];
        camera_matrix_(0, 1) = K[1];
        camera_matrix_(0, 2) = K[2];
        camera_matrix_(1, 0) = K[3];
        camera_matrix_(1, 1) = K[4];
        camera_matrix_(1, 2) = K[5];
        camera_matrix_(2, 0) = K[6];
        camera_matrix_(2, 1) = K[7];
        camera_matrix_(2, 2) = K[8];
    }

    // Tuning GUI
    bool tuning_on = false;
    oat::config::getValue<bool>(vm, config_table, "tune", tuning_on);

    if (tuning_on) {

        tuner_ = oat::make_unique<Tuner>(name_);

        TUNE<int>(&dp_->adaptiveThreshWinSizeMin ,
                  "Thresh min. window size (px)",
                  3.0,
                  10.0,
                  dp_->adaptiveThreshWinSizeMin,
                  1.0);
        TUNE<int>(&dp_->adaptiveThreshWinSizeMax,
                  "Thresh max. window size (px)",
                  10.0,
                  50.0,
                  dp_->adaptiveThreshWinSizeMax,
                  1.0);
        TUNE<int>(&dp_->adaptiveThreshWinSizeStep,
                  "Thresh step size (px)",
                  1.0,
                  20.0,
                  dp_->adaptiveThreshWinSizeStep,
                  1.0);
        TUNE<double>(&dp_->minMarkerPerimeterRate,
                     "Min contour (% width)",
                     0.01,
                     1.00,
                     dp_->minMarkerPerimeterRate,
                     100);
        TUNE<double>(&dp_->maxMarkerPerimeterRate,
                     "Max contour (% width)",
                     0.25,
                     4.00,
                     dp_->maxMarkerPerimeterRate,
                     100);
        TUNE<double>(&dp_->maxErroneousBitsInBorderRate,
                     "Border error (%)",
                     0.0,
                     1.0,
                     dp_->maxErroneousBitsInBorderRate,
                     100);
        TUNE<double>(&dp_->minCornerDistanceRate,
                     "Min corner dist (cm)",
                     0.0,
                     1.0,
                     dp_->minCornerDistanceRate,
                     100);
        TUNE<double>(&dp_->minMarkerDistanceRate,
                     "Min marker dist (cm)",
                     0.0,
                     1.0,
                     dp_->minMarkerDistanceRate,
                     100);
        TUNE<int>(&dp_->minDistanceToBorder,
                  "Min border dist (px)",
                  1.0,
                  20.0,
                  dp_->minDistanceToBorder,
                  1.0);
        TUNE<int>(&dp_->perspectiveRemovePixelPerCell,
                  "Pixels per cell (px)",
                  1,
                  10.0,
                  dp_->perspectiveRemovePixelPerCell,
                  1);
    }
}

oat::Pose ArucoBoard::detectPose(oat::Frame &frame)
{
    oat::Pose pose(
        Pose::DistanceUnit::Meters, Pose::DOF::Three, Pose::DOF::Three);

    std::vector<int> marker_ids;
    Corners marker_corners;
    Corners rejected_corners;
    cv::aruco::detectMarkers(frame,
                             board_->dictionary,
                             marker_corners,
                             marker_ids,
                             dp_,
                             rejected_corners);

    if (refine_detection_) {
        cv::aruco::refineDetectedMarkers(frame,
                                         board_,
                                         marker_corners,
                                         marker_ids,
                                         rejected_corners,
                                         camera_matrix_,
                                         dist_coeff_);
    }

    // Next try to estimate pose
    if (marker_ids.size() > 0) {

        cv::Vec3d rvec, tvec;
        int valid = cv::aruco::estimatePoseBoard(marker_corners,
                                                 marker_ids,
                                                 board_,
                                                 camera_matrix_,
                                                 dist_coeff_,
                                                 rvec,
                                                 tvec,
                                                 false);
        if (valid) {

            // Set rotation and translation vectors and pose confidence
            pose.found = true;
            pose.set_orientation(rvec);
            pose.set_position(tvec);
        }
    }

    if (tuner_) {

        if (marker_corners.size() > 0) {
            cv::aruco::drawDetectedMarkers(frame, marker_corners, marker_ids);
        }

        if (rejected_corners.size() > 0) {
            cv::aruco::drawDetectedMarkers(
                frame, rejected_corners, cv::noArray(), cv::Scalar(0, 0, 255));
        }

        tuner_->tune(frame, pose, camera_matrix_, dist_coeff_);
    }

    return pose;
}

int arucoDictionaryID(const std::string &key)
{
    std::map<std::string, int> dict_map {
        {"4X4_50"  , cv::aruco::DICT_4X4_50  },
        {"4X4_100" , cv::aruco::DICT_4X4_100 },
        {"4X4_250" , cv::aruco::DICT_4X4_250 },
        {"4X4_1000", cv::aruco::DICT_4X4_1000},
        {"5X5_50"  , cv::aruco::DICT_5X5_50  },
        {"5X5_100" , cv::aruco::DICT_5X5_100 },
        {"5X5_250" , cv::aruco::DICT_5X5_250 },
        {"5X5_1000", cv::aruco::DICT_5X5_1000},
        {"6X6_50"  , cv::aruco::DICT_6X6_50  },
        {"6X6_100" , cv::aruco::DICT_6X6_100 },
        {"6X6_250" , cv::aruco::DICT_6X6_250 },
        {"6X6_1000", cv::aruco::DICT_6X6_1000},
        {"7X7_50"  , cv::aruco::DICT_7X7_50  },
        {"7X7_100" , cv::aruco::DICT_7X7_100 },
        {"7X7_250" , cv::aruco::DICT_7X7_250 },
        {"7X7_1000", cv::aruco::DICT_7X7_1000}
    };

    try {
        return dict_map.at(key);
    } catch (const std::out_of_range &ex) {
        throw std::runtime_error("Invalid aruco board dictionary.");
    }
}

} /* namespace oat */
