#ifndef MANAGER_H
#define MANAGER_H
#include <cmath>
#include <vector>
#include <unordered_map>
#include "configor/json.hpp"

#include "character.h"
#include "struction.h"
#include "stroke.h"
#include "config.h"
#include "dot.h"
#include "segment.h"
#include "info.h"
#include "utils.h"
#include "exceptions.h"
//表示笔画评论，语音，笔画序号
class StrokeCommentMessageInfo 
{
public:
    std::string comment;
    std::vector<std::string> sound_array;
    int id;
}
//表示部件评论，语音，笔画序号
class StructionCommentMessageInfo
{
public:
    std::string comment;
    std::vector<std::string> sound_array;
    std::vector<int> id_array;
}
class Manager
{
public:
    Manager(Config config)
    {
        m_config = config;
    }
    void init()
    {
        m_evaluate_character.set_manager(this);
        m_standard_character.set_manager(this);
    }
    std::vector<int> get_struction_segments_index(Struction struction)
    {
        std::vector<int> segment_index_array;
        auto strokes = struction.m_strokes;
        for (auto stroke : strokes)
        {
            auto segments = stroke.m_segments;
            for (auto segment : segments)
            {
                segment_index_array.push_back(segment.index);
            }
        }

        return segment_index_array;
    }
    /**
     * @brief 发送笔画段文件,从网络上获取笔画段与笔画的映射,同时获取了笔顺信息和汉字
     *
     */
    void get_stroke_map(Character &ch, std::vector<Segment> segments, CharacterInfo char_info, std::vector<StructionInfo> struction_info_array, std::vector<StrokeInfo> stroke_info_array, bool is_standard)
    {
        // is_standard==true:构造标准字,假定标准字
        // is_standard==false:构造测试字

        std::vector<Stroke> stroke_array;
        std::vector<Struction> struction_array;
        if (is_standard)
        {
            if (segments.size() != stroke_info_array.size())
            {
                throw StandardException();
            }
        }
        // try
        // {
        for (auto stroke_info : stroke_info_array)
        {
            if (stroke_info.is_skip)
            {
                continue;
            }
            Stroke stroke;
            stroke.set_manager(this);
            stroke.name = stroke_info.name;
            stroke.order = stroke_info.order; //标准字的order
            stroke.is_valid = stroke_info.is_valid;

            if (!is_standard)
            {

                std::transform(stroke_info.segment_index_array.begin(), stroke_info.segment_index_array.end(), std::back_inserter(stroke.m_segments), [segments](auto i)
                               { return segments[i]; });
                stroke.is_reliable = stroke_info.is_reliable;
            }
            else
            {
                stroke.m_segments.push_back(segments[stroke.order]);
                stroke.is_reliable = true;
            }
            stroke_array.push_back(stroke);
        }
        if (!struction_info_array.empty())
        {
            for (auto struction_index : char_info.struction_index_array)
            {
                Struction struction;
                struction.set_manager(this);
                std::transform(struction_info_array[struction_index].stroke_index_array.begin(), struction_info_array[struction_index].stroke_index_array.end(), std::back_inserter(struction.m_strokes), [stroke_array](auto i)
                               { return stroke_array[i]; });
                struction_array.push_back(struction);
            }
            ch.m_structions = struction_array;
        }

        ch.m_strokes = stroke_array;
        // }
        // catch(const std::exception& e)
        // {
        //     std::cerr << e.what() << '\n';
        //     ch.m_strokes = std::vector<Stroke>();
        // }

        ch.m_segments = segments;
        ch.name = char_info.name;
        ch.type = char_info.type;
    }

    std::vector<Segment> load_from_content(std::vector<std::string> lines, Config config)
    {
        std::vector<Segment> segments;
        std::vector<configor::json> line_obj_array;
        std::transform(lines.begin(), lines.end(), std::back_inserter(line_obj_array), [](auto x)
                       { return configor::json::parse(x); });

        for (auto i = 0; i < line_obj_array.size(); ++i)
        {
            auto line_obj = line_obj_array[i];
            Segment segment;
            segment.set_manager(this);
            auto start_x = line_obj["startX"].as_float();
            auto end_x = line_obj["endX"].as_float();
            auto start_y = line_obj["startY"].as_float();
            auto end_y = line_obj["endY"].as_float();
            auto width = end_x - start_x;
            auto height = end_y - start_y;
            auto list = line_obj["list"];
            std::vector<cv::Point2i> points;
            if (width == 0 || height == 0)
            {
                throw ZeroException();
            }
            for (auto item : list)
            {
                auto x = item["x"].as_float();
                auto y = item["y"].as_float();
                auto dx = x - start_x;
                auto dy = y - start_y;
                auto dx_resize = (int)(dx * config.m_data["character"]["width"].as_float() / width);
                auto dy_resize = (int)(dy * config.m_data["character"]["height"].as_float() / height);
                points.push_back(cv::Point2i(dx_resize, dy_resize));
            }
            segment.load_data(points);
            segment.index = i;
            segments.push_back(segment);
        }
        return segments;
    }
    std::vector<Segment> load_from_file(std::string character_file_name, Config config)
    {

        auto lines = dot.load_file(character_file_name);
        return load_from_content(lines, config);
    }

    bool is_stroke_valid(cv::Mat mat)
    {
        //求面积,面积过小,不考虑
        auto mode = cv::RETR_EXTERNAL;
        auto method = cv::CHAIN_APPROX_NONE;
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(mat, contours, mode, method);
        std::vector<int> area_array;
        for (auto contour : contours)
        {
            auto area = cv::contourArea(contour);
            area_array.push_back(area);
        }
        auto area = std::accumulate(area_array.begin(), area_array.end(), 0);
        if (area < 3)
        {
            return false;
        }
        else
        {
            return true;
        }
    }

    std::tuple<
        double, 
        std::unordered_map<std::string, double>, 
        std::unordered_map<std::string, std::string>, 
        std::unordered_map<std::string, int>, 
        std::unordered_map<std::string, double>, 
        std::unordered_map<std::string, std::vector<std::string>>> score(Stroke standard_stroke, Stroke evaluate_stroke, Config config)
    {

        auto total_score_deducted = 0.0;
        std::unordered_map<std::string, std::string> comments;
        std::unordered_map<std::string, std::vector<std::string>> comments_sound;
        std::unordered_map<std::string, int> values;
        std::unordered_map<std::string, double> scores;
        std::unordered_map<std::string, double> full_scores;
        
        auto comment = "";
        auto character_width = config.m_data["character"]["width"].as_float();
        auto character_height = config.m_data["character"]["height"].as_float();
        auto stroke_position_config = config.m_data["stroke_position"];

        auto stroke_size_config = config.m_data["stroke_size"];
        auto stroke_angle_config = config.m_data["stroke_angle"];
        std::vector<std::string> angle_name_array{"横", "横钩", "竖", "竖钩", "弯钩", "竖提", "捺", "斜钩", "撇", "提"};

        std::vector<std::string> position_name_array{"横", "横钩", "横折", "竖折", "竖", "竖钩", "弯钩", "竖提", "捺", "斜钩", "撇", "提"};
        std::vector<std::string> size_name_array{"横", "横钩", "竖", "竖钩", "弯钩", "竖提", "捺", "斜钩", "撇", "提"};
        //轮廓
        auto [standard_mat, standard_stroke_width] = standard_stroke.get_stroke_part(character_width, character_height);
        auto [evaluate_mat, evaluate_stroke_width] = evaluate_stroke.get_stroke_part(character_width, character_height);
        std::string comment_type("stroke_position");
        full_scores.insert({comment_type, config.get_full_score(comment_type)});
        auto standard_stroke_name = standard_stroke.name;
        if (character_height == 0)
        {
            throw ZeroException();
        }
        if (std::find(position_name_array.begin(), position_name_array.end(), standard_stroke_name) != position_name_array.end())
        {

            auto standard_rect = get_rect(standard_stroke.draw(character_width, character_height));
            auto evaluate_rect = get_rect(evaluate_stroke.draw(character_width, character_height));

            if (evaluate_rect.top < standard_rect.top)
            {
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, (evaluate_rect.top - standard_rect.top) / character_height, 3, standard_stroke.order + 1, standard_stroke_name);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            else if (evaluate_rect.top > standard_rect.top)
            {
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, (evaluate_rect.top - standard_rect.top) / character_height, 4, standard_stroke.order + 1, standard_stroke_name);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            else
            {
                comments.insert(std::pair(comment_type, ""));
                values.insert(std::pair(comment_type, 0));
                scores.insert(std::pair(comment_type, 0));
            }
        }
        comment_type = "stroke_angle";
        full_scores.insert({comment_type, config.get_full_score(comment_type)});
        if (std::find(angle_name_array.begin(), angle_name_array.end(), standard_stroke_name) != angle_name_array.end())
        {
            //            std::string name_standard("stroke_angle_standard.png");
            //            cv::imwrite(name_standard, standard_mat);
            //            std::string name_evaluate("stroke_angle_evaluate.png");
            //            cv::imwrite(name_evaluate, evaluate_mat);

            auto angle_info = get_angle_info_half(standard_mat, evaluate_mat);
            if (angle_info.diff_half_angle < 0)
            {
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, angle_info.diff_half_angle, 1, standard_stroke.order + 1, standard_stroke_name);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            else if (angle_info.diff_half_angle > 0)
            {
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, angle_info.diff_half_angle, 2, standard_stroke.order + 1, standard_stroke_name);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            else
            {
                comments.insert(std::pair(comment_type, ""));
                values.insert(std::pair(comment_type, 0));
                scores.insert(std::pair(comment_type, 0));
            }
        }
        comment_type = "stroke_size";
        full_scores.insert({comment_type, config.get_full_score(comment_type)});
        if (std::find(size_name_array.begin(), size_name_array.end(), standard_stroke_name) != size_name_array.end())
        {

            auto standard_rot_rect = get_min_rect(standard_stroke.draw(character_width, character_height));
            auto evaluate_rot_rect = get_min_rect(evaluate_stroke.draw(character_width, character_height));
            auto standard_size = standard_rot_rect.size;
            auto evaluate_size = evaluate_rot_rect.size;
            auto standard_length = std::max(standard_size.width, standard_size.height);
            auto evaluate_length = std::max(evaluate_size.width, evaluate_size.height);
            auto value = (double)evaluate_length / standard_length;
            if (value < 1)
            {
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, 1 - value, 2, standard_stroke.order + 1, standard_stroke_name);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            else if (value > 1)
            {
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, 1 - 1 / value, 1, standard_stroke.order + 1, standard_stroke_name);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            else
            {
                comments.insert(std::pair(comment_type, ""));
                values.insert(std::pair(comment_type, 0));
                scores.insert(std::pair(comment_type, 0));
            }
        }
        return {total_score_deducted, scores, comments, values, full_scores, comments_sound};
    }
    std::tuple<
        double, 
        std::unordered_map<std::string, double>, 
        std::unordered_map<std::string, std::string>, 
        std::unordered_map<std::string, int>, 
        std::unordered_map<std::string, double>, 
        std::unordered_map<std::string, double>, 
        std::unordered_map<std::string, std::vector<std::string>>> score(Struction standard_struction, Struction evaluate_struction, Config config)
    {

        auto total_score_deducted = 0.0;
        std::unordered_map<std::string, std::string> comments;
        std::unordered_map<std::string, std::vector<std::string>> comments_sound;
        std::unordered_map<std::string, int> values;
        std::unordered_map<std::string, double> scores;
        std::unordered_map<std::string, double> full_scores;
        std::unordered_map<std::string, double> double_values;
        auto comment = "";
        auto character_width = config.m_data["character"]["width"].as_float();
        auto character_height = config.m_data["character"]["height"].as_float();
        auto struction_position_config = config.m_data["stuction_position"];
        auto struction_scale_config = config.m_data["structon_scale"];
        auto struction_size_config = config.m_data["stuction_size"];
        auto struction_angle_config = config.m_data["stuction_angle"];
        auto [position_info, size_info] = get_position_size_info(standard_struction.draw(character_width, character_height), evaluate_struction.draw(character_width, character_height), character_width, character_height);
        auto [position_info_rot, size_info_rot] = get_position_size_info_rot(standard_struction.draw(character_width, character_height), evaluate_struction.draw(character_width, character_height), 45, character_width, character_height);
        // position
        //实验,要测试得知
        std::string comment_type("struction_position");
        full_scores.insert({comment_type, config.get_full_score(comment_type)});
        auto is_value_valid = false;
        if (position_info_rot.diff_center_x < 0)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, position_info_rot.diff_center_x, 5, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
                scores.insert(std::pair(comment_type, _score));
                is_value_valid = is_value_valid || (_value != 0);
            }
        }
        if (position_info_rot.diff_center_x > 0)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, position_info_rot.diff_center_x, 8, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
                scores.insert(std::pair(comment_type, _score));
                is_value_valid = is_value_valid || (_value != 0);
            }
        }
        if (position_info_rot.diff_center_y < 0)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, position_info_rot.diff_center_y, 7, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
                scores.insert(std::pair(comment_type, _score));
                is_value_valid = is_value_valid || (_value != 0);
            }
        }
        if (position_info_rot.diff_center_y > 0)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, position_info_rot.diff_center_y, 6, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
                scores.insert(std::pair(comment_type, _score));
                is_value_valid = is_value_valid || (_value != 0);
            }
        }
        if (!is_value_valid)
        {
            if (position_info.diff_center_x < 0)
            {
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, position_info.diff_center_x, 1, 0);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                    is_value_valid = is_value_valid || (_value != 0);
                }
            }
            if (position_info.diff_center_x > 0)
            {
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, position_info.diff_center_x, 2, 0);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            if (position_info.diff_center_y < 0)
            {
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, position_info.diff_center_y, 3, 0);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                    is_value_valid = is_value_valid || (_value != 0);
                }
            }
            if (position_info.diff_center_y > 0)
            {
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, position_info.diff_center_y, 4, 0);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                    is_value_valid = is_value_valid || (_value != 0);
                }
            }
            if (!is_value_valid)
            {
                comments.insert(std::pair(comment_type, ""));
                values.insert(std::pair(comment_type, 0));
                scores.insert(std::pair(comment_type, 0));
            }
        }

        // size
        comment_type = "struction_size";
        full_scores.insert({comment_type, config.get_full_score(comment_type)});
        if (size_info.width_ratio * size_info.height_ratio == 0)
        {
            throw ZeroException();
        }
        if (size_info.width_ratio < 0.8 && size_info.height_ratio < 0.8)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, 1 - max_value(size_info.width_ratio * size_info.height_ratio), 1, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
            }
            // size_score_info_array.push_back({
            //     1,size_info.width_ratio*size_info.height_ratio,0
            // });
        }
        else if (size_info.width_ratio > 1.2 && size_info.height_ratio > 1.2)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, 1 - max_value(1 / (size_info.width_ratio * size_info.height_ratio)), 2, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
            }
            // size_score_info_array.push_back({
            //     2,1/(size_info.width_ratio*size_info.height_ratio),0
            // });
        }
        else
        {
            comments.insert(std::pair(comment_type, ""));
            values.insert(std::pair(comment_type, 0));
            scores.insert(std::pair(comment_type, 0));
        }

        comment_type = "struction_scale";
        full_scores.insert({comment_type, config.get_full_score(comment_type)});
        if (size_info.width_ratio > 0.9 && size_info.width_ratio <= 1.1 && size_info.height_ratio > 1.1)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, 1 - max_value(1 / size_info.height_ratio), 1, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
                scores.insert(std::pair(comment_type, _score));
            }
            // scale_score_info_array.push_back({
            //     1,1/size_info.height_ratio,0
            // });
        }
        else if (size_info.height_ratio > 0.9 && size_info.height_ratio <= 1.1 && size_info.width_ratio > 1.1)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, 1 - max_value(1 / size_info.width_ratio), 3, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
                scores.insert(std::pair(comment_type, _score));
            }
            // scale_score_info_array.push_back({
            //     3,1/size_info.width_ratio,0
            // });
        }
        else if (size_info.height_ratio > 0.9 && size_info.height_ratio <= 1.1 && size_info.width_ratio < 0.9)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, 1 - max_value(size_info.width_ratio), 8, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
                scores.insert(std::pair(comment_type, _score));
            }
            // scale_score_info_array.push_back({
            //     8,size_info.width_ratio,0
            // });
        }
        else if (size_info.width_ratio > 0.9 && size_info.width_ratio <= 1.1 && size_info.height_ratio < 0.9)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, 1 - max_value(size_info.height_ratio), 2, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
                scores.insert(std::pair(comment_type, _score));
            }
            // scale_score_info_array.push_back({
            //     2,size_info.height_ratio,0
            // });
        }
        else if (size_info.width_ratio < 0.9 && size_info.height_ratio > 1.1)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, 1 - max_value(size_info.width_ratio / size_info.height_ratio), 9, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
                scores.insert(std::pair(comment_type, _score));
            }
            // scale_score_info_array.push_back({
            //     2,size_info.height_ratio,0
            // });
        }
        else if (size_info.height_ratio < 0.9 && size_info.width_ratio > 1.1)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, 1 - max_value(size_info.height_ratio / size_info.width_ratio), 10, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
                scores.insert(std::pair(comment_type, _score));
            }
            // scale_score_info_array.push_back({
            //     2,size_info.height_ratio,0
            // });
        }
        else
        {
            comments.insert(std::pair(comment_type, ""));
            values.insert(std::pair(comment_type, 0));
            scores.insert(std::pair(comment_type, 0));
        }
        comment_type = "struction_angle";
        full_scores.insert({comment_type, config.get_full_score(comment_type)});
        auto [diff_half_angle, diff_angle] = get_angle_info_half(standard_struction.draw(character_width, character_height), evaluate_struction.draw(character_width, character_height));
        if (diff_half_angle < 0)
        {
            //设为左
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, diff_half_angle, 1, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
            }
        }
        else if (diff_half_angle > 0)
        {
            //设为右
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, diff_half_angle, 2, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
            }
        }
        else
        {
            comments.insert(std::pair(comment_type, ""));
            values.insert(std::pair(comment_type, 0));
            scores.insert(std::pair(comment_type, 0));
        }
        double_values.insert(std::pair(comment_type, diff_half_angle));
        return {total_score_deducted, scores, comments, values, full_scores, double_values, comments_sound};
    }

    std::tuple<
        double, 
        std::unordered_map<std::string, double>, 
        std::unordered_map<std::string, std::string>, 
        std::unordered_map<std::string, int>, 
        std::unordered_map<std::string, double>, 
        std::unordered_map<std::string, std::vector<std::string>>> score(Character standard_character, Character evaluate_character, std::vector<int> struction_angle_result, std::vector<double> struction_angle_value, Config config)
    {
        // struction_angle_result:结构的评测结果
        //
        auto total_score_deducted = 0.0;
        std::unordered_map<std::string, std::string> comments;
        std::unordered_map<std::string, std::vector<std::string>> comments_sound;
        std::unordered_map<std::string, int> values;
        std::unordered_map<std::string, double> scores;
        std::unordered_map<std::string, double> full_scores;
        // auto comment = "";
        //  std::vector<PositionScoreInfo> position_score_info_array;
        //  std::vector<SizeScoreInfo> size_score_info_array;
        //  std::vector<ScaleScoreInfo> scale_score_info_array;
        //  std::vector<AngleScoreInfo> angle_score_info_array;
        auto character_width = config.m_data["character"]["width"].as_float();
        auto character_height = config.m_data["character"]["height"].as_float();
        auto character_position_config = config.m_data["character_position"];
        auto character_scale_config = config.m_data["character_scale"];
        auto character_size_config = config.m_data["character_size"];
        auto character_angle_config = config.m_data["character_angle"];
        auto [position_info, size_info] = get_position_size_info(standard_character.draw(character_width, character_height), evaluate_character.draw(character_width, character_height), character_width, character_height);
        auto [position_info_rot, size_info_rot] = get_position_size_info_rot(standard_character.draw(character_width, character_height), evaluate_character.draw(character_width, character_height), 45, character_width, character_height);
        // position
        //实验,要测试得知
        if (size_info.width_ratio * size_info.height_ratio == 0)
        {
            throw ZeroException();
        }
        std::string comment_type("character_position");
        auto is_value_valid = false; //如过下面的_value有效,该数为true
        full_scores.insert({comment_type, config.get_full_score(comment_type)});

        if (position_info_rot.diff_center_x < 0)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, position_info_rot.diff_center_x, 5, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
                scores.insert(std::pair(comment_type, _score));
                is_value_valid = is_value_valid || (_value != 0);
            }
            // position_score_info_array.push_back({
            //     5,position_info_rot.diff_center_x,0
            // });
        }
        if (position_info_rot.diff_center_x > 0)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, position_info_rot.diff_center_x, 8, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
                scores.insert(std::pair(comment_type, _score));
                is_value_valid = is_value_valid || (_value != 0);
            }
            // position_score_info_array.push_back({
            //     6,position_info_rot.diff_center_x,0
            // });
        }
        if (position_info_rot.diff_center_y < 0)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, position_info_rot.diff_center_y, 7, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
                scores.insert(std::pair(comment_type, _score));
                is_value_valid = is_value_valid || (_value != 0);
            }
            // position_score_info_array.push_back({
            //     7,position_info_rot.diff_center_y,0
            // });
        }
        if (position_info_rot.diff_center_y > 0)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, position_info_rot.diff_center_y, 6, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
                scores.insert(std::pair(comment_type, _score));
                is_value_valid = is_value_valid || (_value != 0);
            }
            // position_score_info_array.push_back({
            //     8,position_info_rot.diff_center_y,0
            // });
        }
        if (!is_value_valid)
        {
            if (position_info.diff_center_x < 0)
            {
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, position_info.diff_center_x, 1, 0);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                    is_value_valid = is_value_valid || (_value != 0);
                }
                // position_score_info_array.push_back({
                //     1,position_info_rot.diff_center_x,0
                // });
            }
            if (position_info.diff_center_x > 0)
            {
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, position_info.diff_center_x, 2, 0);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                    is_value_valid = is_value_valid || (_value != 0);
                }
                // position_score_info_array.push_back({
                //     2,position_info_rot.diff_center_x,0
                // });
            }
            if (position_info.diff_center_y < 0)
            {
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, position_info.diff_center_y, 3, 0);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                    is_value_valid = is_value_valid || (_value != 0);
                }
                // position_score_info_array.push_back({
                //     3,position_info_rot.diff_center_y,0
                // });
            }
            if (position_info.diff_center_y > 0)
            {
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, position_info.diff_center_y, 4, 0);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                    is_value_valid = is_value_valid || (_value != 0);
                }
                // position_score_info_array.push_back({
                //     4,position_info_rot.diff_center_y,0
                // });
            }
            if (!is_value_valid)
            {
                comments.insert(std::pair(comment_type, ""));
                values.insert(std::pair(comment_type, 0));
                scores.insert(std::pair(comment_type, 0));
            }
        }

        // size
        comment_type = "character_size";
        full_scores.insert({comment_type, config.get_full_score(comment_type)});

        if (size_info.width_ratio < 0.9 && size_info.height_ratio < 0.9)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, 1 - max_value(size_info.width_ratio * size_info.height_ratio), 1, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
                scores.insert(std::pair(comment_type, _score));
            }
            // size_score_info_array.push_back({
            //     1,size_info.width_ratio*size_info.height_ratio,0
            // });
        }
        else if (size_info.width_ratio > 1.1 && size_info.height_ratio > 1.1)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, 1 - max_value(1 / (size_info.width_ratio * size_info.height_ratio)), 2, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
                scores.insert(std::pair(comment_type, _score));
            }
            // size_score_info_array.push_back({
            //     2,1/(size_info.width_ratio*size_info.height_ratio),0
            // });
        }
        else
        {
            comments.insert(std::pair(comment_type, ""));
            values.insert(std::pair(comment_type, 0));
            scores.insert(std::pair(comment_type, 0));
        }
        // scale
        comment_type = "character_scale";
        full_scores.insert({comment_type, config.get_full_score(comment_type)});
        if (size_info.width_ratio > 0.9 && size_info.width_ratio <= 1.1 && size_info.height_ratio > 1.1)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, 1 - max_value(1 / size_info.height_ratio), 1, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
                scores.insert(std::pair(comment_type, _score));
            }
            // scale_score_info_array.push_back({
            //     1,1/size_info.height_ratio,0
            // });
        }
        else if (size_info.height_ratio > 0.9 && size_info.height_ratio <= 1.1 && size_info.width_ratio > 1.1)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, 1 - max_value(1 / size_info.width_ratio), 3, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
                scores.insert(std::pair(comment_type, _score));
            }
            // scale_score_info_array.push_back({
            //     3,1/size_info.width_ratio,0
            // });
        }
        else if (size_info.height_ratio > 0.9 && size_info.height_ratio <= 1.1 && size_info.width_ratio < 0.9)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, 1 - max_value(size_info.width_ratio), 8, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
                scores.insert(std::pair(comment_type, _score));
            }
            // scale_score_info_array.push_back({
            //     8,size_info.width_ratio,0
            // });
        }
        else if (size_info.width_ratio > 0.9 && size_info.width_ratio <= 1.1 && size_info.height_ratio < 0.9)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, 1 - max_value(size_info.height_ratio), 2, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
                scores.insert(std::pair(comment_type, _score));
            }
            // scale_score_info_array.push_back({
            //     2,size_info.height_ratio,0
            // });
        }
        else if (size_info.width_ratio < 0.9 && size_info.height_ratio > 1.1)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, 1 - max_value(size_info.width_ratio / size_info.height_ratio), 9, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
                scores.insert(std::pair(comment_type, _score));
            }
            // scale_score_info_array.push_back({
            //     2,size_info.height_ratio,0
            // });
        }
        else if (size_info.height_ratio < 0.9 && size_info.width_ratio > 1.1)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, 1 - max_value(size_info.height_ratio / size_info.width_ratio), 10, 0);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
                scores.insert(std::pair(comment_type, _score));
            }
            // scale_score_info_array.push_back({
            //     2,size_info.height_ratio,0
            // });
        }
        else
        {
            comments.insert(std::pair(comment_type, ""));
            values.insert(std::pair(comment_type, 0));
            scores.insert(std::pair(comment_type, 0));
        }
        //角度:
        //非独体字:部件中心角度
        //独体字:上下/左右半部分角度
        //⿰⿱⿲⿳⿴⿵⿶⿷⿸⿹⿺⿻

        comment_type = "character_angle";
        full_scores.insert({comment_type, config.get_full_score(comment_type)});
        switch (hash_(standard_character.type))
        {
        case hash_compile_time(" "):
        {
            //独体
            //上下/左右半部分角度判断是否角度倾斜
            //调用部件评测函数
            auto [diff_half_angle, diff_angle] = get_angle_info_half(standard_character.draw(character_width, character_height), evaluate_character.draw(character_width, character_height));
            if (diff_half_angle < 0)
            {
                //设为左
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, diff_half_angle, 1, 0);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            else if (diff_half_angle > 0)
            {
                //设为右
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, diff_half_angle, 2, 0);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            else
            {
                comments.insert(std::pair(comment_type, ""));
                values.insert(std::pair(comment_type, 0));
                scores.insert(std::pair(comment_type, 0));
            }
            break;
        }
        case hash_compile_time("⿰"):
        case hash_compile_time("⿱"):
        {
            //左右
            //两个部件均倾斜
            //左右两个部件重心连线倾斜

            //上下
            //两个部件均倾斜
            //上下两个部件重心连线倾斜
            if (evaluate_character.m_structions.empty())
            {
                comments.insert(std::pair(comment_type, ""));
                values.insert(std::pair(comment_type, 0));
                scores.insert(std::pair(comment_type, 0));
                break;
            }
            auto left_struction_result = struction_angle_result[0];
            auto right_struction_result = struction_angle_result[1];
            if (left_struction_result == 1 && right_struction_result == 1)
            {
                auto min_value_iter = std::min_element(struction_angle_value.begin(), struction_angle_value.end(), [](auto x, auto y)
                                                       { return abs(x) < abs(y); });
                auto value = *min_value_iter;
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, value, 1, 0);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            else if (left_struction_result == 2 && right_struction_result == 2)
            {
                auto min_value_iter = std::min_element(struction_angle_value.begin(), struction_angle_value.end(), [](auto x, auto y)
                                                       { return abs(x) < abs(y); });
                auto value = *min_value_iter;
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, value, 2, 0);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            else if (left_struction_result == 1 && right_struction_result == 2)
            {
                auto min_value_iter = std::min_element(struction_angle_value.begin(), struction_angle_value.end(), [](auto x, auto y)
                                                       { return abs(x) < abs(y); });
                auto value = *min_value_iter;
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, value, 3, 0);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            else if (left_struction_result == 2 && right_struction_result == 1)
            {
                auto min_value_iter = std::min_element(struction_angle_value.begin(), struction_angle_value.end(), [](auto x, auto y)
                                                       { return abs(x) < abs(y); });
                auto value = *min_value_iter;
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, value, 3, 0);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            // else
            // {
            //     comments.insert(std::pair(comment_type, ""));
            //     values.insert(std::pair(comment_type, 0));
            //     scores.insert(std::pair(comment_type, 0));
            //}
            else 
            {
                auto standard_character_structions = standard_character.m_structions;
                auto evaluate_character_structions = evaluate_character.m_structions;
                auto left_standard_character_struction = standard_character_structions[0];
                auto right_standard_character_struction = standard_character_structions[1];
                auto left_evaluate_character_struction = evaluate_character_structions[0];
                auto right_evaluate_charcter_struction = evaluate_character_structions[1];
                auto left_standard_struction_rect = get_rect(left_standard_character_struction.draw(character_width, character_height));
                auto right_standard_struction_rect = get_rect(right_standard_character_struction.draw(character_width, character_height));
                auto left_evaluate_struction_rect = get_rect(left_evaluate_character_struction.draw(character_width, character_height));
                auto right_evaluate_struction_rect = get_rect(right_evaluate_charcter_struction.draw(character_width, character_height));
                auto standard_angle = atan2(
                    right_standard_struction_rect.center_y - left_standard_struction_rect.center_y,
                    right_standard_struction_rect.center_x - left_standard_struction_rect.center_x);
                auto evaluate_angle = atan2(
                    right_evaluate_struction_rect.center_y - left_evaluate_struction_rect.center_y,
                    right_evaluate_struction_rect.center_x - left_evaluate_struction_rect.center_x);
                auto diff_angle = (evaluate_angle - standard_angle) / M_PI;
                if (diff_angle < 0)
                {
                    auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, diff_angle, 1, 0);
                    if (_value != 0)
                    {
                        total_score_deducted += _score;
                        comments.insert(std::pair(comment_type, _comment));
                        comments_sound.insert(std::pair(comment_type, _sound));
                        values.insert(std::pair(comment_type, _value));
                        scores.insert(std::pair(comment_type, _score));
                    }
                }
                else if (diff_angle > 0)
                {
                    auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, diff_angle, 2, 0);
                    if (_value != 0)
                    {
                        total_score_deducted += _score;
                        comments.insert(std::pair(comment_type, _comment));
                        comments_sound.insert(std::pair(comment_type, _sound));
                        values.insert(std::pair(comment_type, _value));
                        scores.insert(std::pair(comment_type, _score));
                    }
                }
                else
                {
                    comments.insert(std::pair(comment_type, ""));
                    values.insert(std::pair(comment_type, 0));
                    scores.insert(std::pair(comment_type, 0));
                }
            }
            
            break;
        }

        case hash_compile_time("⿲"):
        case hash_compile_time("⿳"):
        {
            //左中右
            //左右两个部件倾斜
            //三个部件的连线中有两个倾斜

            //上中下
            //上下两个部件倾斜
            //三个部件的连线中有两个倾斜
            if (evaluate_character.m_structions.empty())
            {
                comments.insert(std::pair(comment_type, ""));
                values.insert(std::pair(comment_type, 0));
                scores.insert(std::pair(comment_type, 0));
                break;
            }
            std::vector<int> result_array_left;
            std::vector<int> result_array_right;
            std::copy_if(struction_angle_result.begin(), struction_angle_result.end(), std::back_inserter(result_array_left), [](auto x)
                         { return x == 1; });
            std::copy_if(struction_angle_result.begin(), struction_angle_result.end(), std::back_inserter(result_array_right), [](auto x)
                         { return x == 2; });
            if (result_array_left.size() >= 2 && result_array_right.size() == 0)
            {
                auto min_value_iter = std::min_element(struction_angle_value.begin(), struction_angle_value.end(), [](auto x, auto y)
                                                       { return abs(x) < abs(y); });
                auto value = *min_value_iter;
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, value, 1, 0);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            else if (result_array_right.size() >= 2 && result_array_left.size() == 0)
            {
                auto min_value_iter = std::min_element(struction_angle_value.begin(), struction_angle_value.end(), [](auto x, auto y)
                                                       { return abs(x) < abs(y); });
                auto value = *min_value_iter;
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, value, 2, 0);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            else if (result_array_right.size() >= 1 && result_array_left.size() >= 1)
            {
                auto min_value_iter = std::min_element(struction_angle_value.begin(), struction_angle_value.end(), [](auto x, auto y)
                                                       { return abs(x) < abs(y); });
                auto value = *min_value_iter;
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, value, 3, 0);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            // else
            // {
            //     // comments.insert({comment_type, ""});
            //     // values.insert(std::pair(comment_type, 0));
            //     // scores.insert(std::pair(comment_type, 0));
            // }
            else 
            {
                auto standard_character_structions = standard_character.m_structions;
                auto evaluate_character_structions = evaluate_character.m_structions;
                std::vector<RectInfo> standard_struction_rect_array;
                std::vector<RectInfo> evaluate_struction_rect_array;
                for (auto struction : standard_character_structions)
                {
                    auto rect = get_rect(struction.draw(character_width, character_height));
                    standard_struction_rect_array.push_back(rect);
                }
                for (auto struction : evaluate_character_structions)
                {
                    auto rect = get_rect(struction.draw(character_width, character_height));
                    evaluate_struction_rect_array.push_back(rect);
                }
                auto standard_angle_01 = atan2(
                    standard_struction_rect_array[1].center_y - standard_struction_rect_array[0].center_y,
                    standard_struction_rect_array[1].center_x - standard_struction_rect_array[0].center_x);
                auto standard_angle_12 = atan2(
                    standard_struction_rect_array[2].center_y - standard_struction_rect_array[1].center_y,
                    standard_struction_rect_array[2].center_x - standard_struction_rect_array[1].center_x);

                auto evaluate_angle_01 = atan2(
                    evaluate_struction_rect_array[1].center_y - evaluate_struction_rect_array[0].center_y,
                    evaluate_struction_rect_array[1].center_x - evaluate_struction_rect_array[0].center_x);
                auto evaluate_angle_12 = atan2(
                    evaluate_struction_rect_array[2].center_y - evaluate_struction_rect_array[1].center_y,
                    evaluate_struction_rect_array[2].center_x - evaluate_struction_rect_array[1].center_x);
                auto diff_angle_01 = (evaluate_angle_01 - standard_angle_01) / M_PI;
                auto diff_angle_12 = (evaluate_angle_12 - standard_angle_12) / M_PI;
                if (diff_angle_01 == 0 && diff_angle_12 == 0)
                {
                    comments.insert(std::pair(comment_type, ""));
                    values.insert(std::pair(comment_type, 0));
                    scores.insert(std::pair(comment_type, 0));
                }
                else if (diff_angle_01 <= 0 && diff_angle_12 <= 0)
                {
                    auto angles = {diff_angle_01, diff_angle_12};
                    auto min_value_iter = std::min_element(angles.begin(), angles.end(), [](auto x, auto y)
                                                        { return abs(x) < abs(y); });
                    auto min_value = *min_value_iter;
                    auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, min_value, 1, 0);
                    if (_value != 0)
                    {
                        total_score_deducted += _score;
                        comments.insert(std::pair(comment_type, _comment));
                        comments_sound.insert(std::pair(comment_type, _sound));
                        values.insert(std::pair(comment_type, _value));
                        scores.insert(std::pair(comment_type, _score));
                    }
                }
                else if (diff_angle_01 >= 0 && diff_angle_12 >= 0)
                {
                    auto angles = {diff_angle_01, diff_angle_12};
                    auto min_value_iter = std::min_element(angles.begin(), angles.end(), [](auto x, auto y)
                                                        { return abs(x) < abs(y); });
                    auto min_value = *min_value_iter;
                    auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, min_value, 2, 0);
                    if (_value != 0)
                    {
                        total_score_deducted += _score;
                        comments.insert(std::pair(comment_type, _comment));
                        comments_sound.insert(std::pair(comment_type, _sound));
                        values.insert(std::pair(comment_type, _value));
                        scores.insert(std::pair(comment_type, _score));
                    }
                }
                else if (diff_angle_01 >= 0 && diff_angle_12 <= 0)
                {
                    auto angles = {diff_angle_01, diff_angle_12};
                    auto min_value_iter = std::min_element(angles.begin(), angles.end(), [](auto x, auto y)
                                                        { return abs(x) < abs(y); });
                    auto min_value = *min_value_iter;
                    auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, min_value, 3, 0);
                    if (_value != 0)
                    {
                        total_score_deducted += _score;
                        comments.insert(std::pair(comment_type, _comment));
                        comments_sound.insert(std::pair(comment_type, _sound));
                        values.insert(std::pair(comment_type, _value));
                        scores.insert(std::pair(comment_type, _score));
                    }
                }
                else if (diff_angle_01 <= 0 && diff_angle_12 >= 0)
                {
                    auto angles = {diff_angle_01, diff_angle_12};
                    auto min_value_iter = std::min_element(angles.begin(), angles.end(), [](auto x, auto y)
                                                        { return abs(x) < abs(y); });
                    auto min_value = *min_value_iter;
                    auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, min_value, 3, 0);
                    if (_value != 0)
                    {
                        total_score_deducted += _score;
                        comments.insert(std::pair(comment_type, _comment));
                        comments_sound.insert(std::pair(comment_type, _sound));
                        values.insert(std::pair(comment_type, _value));
                        scores.insert(std::pair(comment_type, _score));
                    }
                }
                else 
                {
                    if (diff_angle_01 < 0)
                    {
                        auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, diff_angle_01, 1, 0);
                        if (_value != 0)
                        {
                            total_score_deducted += _score;
                            comments.insert(std::pair(comment_type, _comment));
                            comments_sound.insert(std::pair(comment_type, _sound));
                            values.insert(std::pair(comment_type, _value));
                            scores.insert(std::pair(comment_type, _score));
                        }
                    }
                    else if (diff_angle_01 > 0)
                    {
                        auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, diff_angle_01, 2, 0);
                        if (_value != 0)
                        {
                            total_score_deducted += _score;
                            comments.insert(std::pair(comment_type, _comment));
                            comments_sound.insert(std::pair(comment_type, _sound));
                            values.insert(std::pair(comment_type, _value));
                            scores.insert(std::pair(comment_type, _score));
                        }
                    }
                    if (diff_angle_12 < 0)
                    {
                        auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, diff_angle_12, 1, 0);
                        if (_value != 0)
                        {
                            total_score_deducted += _score;
                            comments.insert(std::pair(comment_type, _comment));
                            comments_sound.insert(std::pair(comment_type, _sound));
                            values.insert(std::pair(comment_type, _value));
                            scores.insert(std::pair(comment_type, _score));
                        }
                    }
                    else if (diff_angle_12 > 0)
                    {
                        auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, diff_angle_12, 2, 0);
                        if (_value != 0)
                        {
                            total_score_deducted += _score;
                            comments.insert(std::pair(comment_type, _comment));
                            comments_sound.insert(std::pair(comment_type, _sound));
                            values.insert(std::pair(comment_type, _value));
                            scores.insert(std::pair(comment_type, _score));
                        }
                    }
                }
                
                
            }
            
            break;
        }

        case hash_compile_time("⿴"):
        case hash_compile_time("⿵"):
        case hash_compile_time("⿶"):
        case hash_compile_time("⿷"):
        {
            //全包围
            //外部框倾斜
            //上三面
            //外部框倾斜
            //下三面
            //外部框倾斜
            //左三面
            //外部框倾斜
            if (evaluate_character.m_structions.empty())
            {
                comments.insert(std::pair(comment_type, ""));
                values.insert(std::pair(comment_type, 0));
                scores.insert(std::pair(comment_type, 0));
                break;
            }
            auto left_struction_result = struction_angle_result[0];
            auto right_struction_result = struction_angle_result[1];
            if (left_struction_result == 1)
            {
                auto value = struction_angle_value[0];
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, value, 1, 0);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            else if (left_struction_result == 2)
            {
                auto value = struction_angle_value[0];
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, value, 2, 0);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            else
            {
                comments.insert({comment_type, ""});
                values.insert(std::pair(comment_type, 0));
                scores.insert(std::pair(comment_type, 0));
            }
            break;
        }

        case hash_compile_time("⿸"):
        case hash_compile_time("⿹"):
        case hash_compile_time("⿺"):
        case hash_compile_time("⿻"):
        {
            //左上两面
            //两个部件均倾斜
            //右上两面
            //两个部件均倾斜
            //左下两面
            //两个部件均倾斜
            //镶嵌
            //两个部件均倾斜
            if (evaluate_character.m_structions.empty())
            {
                comments.insert(std::pair(comment_type, ""));
                values.insert(std::pair(comment_type, 0));
                scores.insert(std::pair(comment_type, 0));
                break;
            }
            auto left_struction_result = struction_angle_result[0];
            auto right_struction_result = struction_angle_result[1];
            if (left_struction_result == 1 && right_struction_result == 1)
            {
                auto min_value_iter = std::min_element(struction_angle_value.begin(), struction_angle_value.end(), [](auto x, auto y)
                                                       { return abs(x) < abs(y); });
                auto value = *min_value_iter;
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, value, 1, 0);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            else if (left_struction_result == 2 && right_struction_result == 2)
            {
                auto min_value_iter = std::min_element(struction_angle_value.begin(), struction_angle_value.end(), [](auto x, auto y)
                                                       { return abs(x) < abs(y); });
                auto value = *min_value_iter;
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, value, 2, 0);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            else if (left_struction_result == 1 && right_struction_result == 2)
            {
                auto min_value_iter = std::min_element(struction_angle_value.begin(), struction_angle_value.end(), [](auto x, auto y)
                                                       { return abs(x) < abs(y); });
                auto value = *min_value_iter;
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, value, 3, 0);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            else if (left_struction_result == 2 && right_struction_result == 1)
            {
                auto min_value_iter = std::min_element(struction_angle_value.begin(), struction_angle_value.end(), [](auto x, auto y)
                                                       { return abs(x) < abs(y); });
                auto value = *min_value_iter;
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, value, 3, 0);
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            else
            {
                comments.insert(std::pair(comment_type, ""));
                values.insert(std::pair(comment_type, 0));
                scores.insert(std::pair(comment_type, 0));
            }
            break;
        }
        }
        return {total_score_deducted, scores, comments, values, full_scores, comments_sound};
    }

    std::vector<Stroke> get_all_strokes(Character character)
    {
        //输出: 按标准字笔画排序后的笔画, 实际笔画
        std::vector<Stroke> all_strokes;
        auto structions = character.m_structions;
        for (auto struction : structions)
        {
            auto strokes = struction.m_strokes;
            for (auto stroke : strokes)
            {
                all_strokes.push_back(stroke);
            }
        }
        std::vector<Stroke> all_strokes_sorted_by_order(all_strokes);
        // std::vector <Stroke> all_strokes_sorted_by_real_order(all_strokes);
        std::sort(all_strokes_sorted_by_order.begin(), all_strokes_sorted_by_order.end(), [](auto x, auto y)
                  { return x.order < y.order; });
        // std::sort(all_strokes_sorted_by_real_order.begin(), all_strokes_sorted_by_real_order.end(), [](auto x, auto y){
        //     return x.real_order<y.real_order;
        // });
        return all_strokes_sorted_by_order;
    }
    std::tuple<
        double, 
        std::unordered_map<std::string, double>, 
        std::unordered_map<std::string, std::string>, 
        std::unordered_map<std::string, int>, 
        std::unordered_map<std::string, double>, 
        std::unordered_map<std::string, std::vector<std::string>>> score_base(
        // std::vector<Stroke> standard_all_strokes_sorted_by_order,
        // std::vector<Stroke> evaluate_all_strokes_sorted_by_order,
        bool is_character_right,
        Config config,
        std::vector<StrokeInfo> evaluate_stroke_info_array,
        std::vector<std::string> standard_lines, //强制比较
        std::vector<std::string> evaluate_lines, //强制比较
        bool is_only_character_right_and_speed = false
        
    )
    {
        // is_only_character_right==true, 只有is_character_right结果保留
        std::unordered_map<std::string, std::string> comments;
        std::unordered_map<std::string, std::vector<std::string>> comments_sound;
        std::unordered_map<std::string, int> values;
        std::unordered_map<std::string, double> scores;
        std::unordered_map<std::string, double> full_scores;
        auto total_score_deducted = 0.0;
        std::string comment_type;
        if (!is_only_character_right_and_speed)
        {
            //笔画数量
            comment_type = "stroke_count";
            full_scores.insert({comment_type, config.get_full_score(comment_type)});
            // auto standard_all_strokes_sorted_by_order = get_all_strokes(standard_character);
            // auto evaluate_all_strokes_sorted_by_order = get_all_strokes(evaluate_character);

            //auto value = evaluate_all_strokes_sorted_by_order.size() - standard_all_strokes_sorted_by_order.size();
            auto stroke_count_diff = -(long)standard_lines.size() + (long)evaluate_lines.size();
            if (stroke_count_diff > 0)
            {

                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, 1, 1, abs(stroke_count_diff));
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            else if (stroke_count_diff < 0)
            {
                auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, 1, 2, abs(stroke_count_diff));
                if (_value != 0)
                {
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    comments_sound.insert(std::pair(comment_type, _sound));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            else
            {
                comments.insert(std::pair(comment_type, ""));
                values.insert(std::pair(comment_type, 0));
                scores.insert(std::pair(comment_type, 0));
            }
            //笔顺
            comment_type = "stroke_order";
            full_scores.insert({comment_type, config.get_full_score(comment_type)});
            bool is_order_right = true;
            auto stroke_order_id = -1;
            
            if (stroke_count_diff!=0)
            {
                auto _comment = "";
                auto _value = 1;
                auto _score = 1.0;
                if (_value != 0)
                {
                    is_order_right = false;
                    total_score_deducted += _score;
                    comments.insert(std::pair(comment_type, _comment));
                    values.insert(std::pair(comment_type, _value));
                    scores.insert(std::pair(comment_type, _score));
                }
            }
            else
            {
                // for (auto evaluate_stroke:evaluate_all_strokes_sorted_by_order)
                // {
                    
                //     if (evaluate_stroke.order!=evaluate_stroke)
                //     {
                //         stroke_order_id = i;
                //     }    
                // }
                // if (stroke_order_id!=-1)
                // {
                //     auto [_score, _comment, _value] = config.get_comment(comment_type, 1, 1, stroke_order_id + 1, stroke.name);
                //     if (_value != 0)
                //     {
                //         is_order_right = false;
                //         total_score_deducted += _score;
                //         comments.insert(std::pair(comment_type, _comment));
                //         values.insert(std::pair(comment_type, _value));
                //         scores.insert(std::pair(comment_type, _score));
                //     }
                // }
                
                for (auto evaluate_stroke_info: evaluate_stroke_info_array)
                {
                    if (evaluate_stroke_info.is_skip)
                    {
                        is_order_right = true;
                        break;
                    }
                
                    if (evaluate_stroke_info.order!=evaluate_stroke_info.segment_index_array[0])
                    {
                        is_order_right = false;
                        stroke_order_id = evaluate_stroke_info.order;
                        break;
                    }
                }
                if (stroke_order_id!=-1)
                {
                    auto stroke_info = evaluate_stroke_info_array[stroke_order_id];
                    auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, 1, 1, stroke_order_id + 1, stroke_info.name);
                    if (_value != 0)
                    {
                        is_order_right = false;
                        total_score_deducted += _score;

                        comments.insert(std::pair(comment_type, _comment));
                        comments_sound.insert(std::pair(comment_type, _sound));
                        values.insert(std::pair(comment_type, _value));
                        scores.insert(std::pair(comment_type, _score));
                    }
                }
            }
            
   
            
            if (is_order_right)
            {
                comments.insert(std::pair(comment_type, ""));
                values.insert(std::pair(comment_type, 0));
                scores.insert(std::pair(comment_type, 0));
            }
        }

        //书写速度
        //暂时不做
        comment_type = "writing_speed";

        // for (auto stroke : evaluate_all_strokes_sorted_by_order)
        // {
        // }
        //错别字
        comment_type = "incorrect_character";
        full_scores.insert({comment_type, config.get_full_score(comment_type)});
        if (!is_character_right)
        {
            auto [_score, _comment, _value, _sound] = config.get_comment(comment_type, 1, 1);
            if (_value != 0)
            {
                total_score_deducted += _score;
                comments.insert(std::pair(comment_type, _comment));
                comments_sound.insert(std::pair(comment_type, _sound));
                values.insert(std::pair(comment_type, _value));
                scores.insert(std::pair(comment_type, _score));
            }
        }
        else
        {
            comments.insert(std::pair(comment_type, ""));
            values.insert(std::pair(comment_type, 0));
            scores.insert(std::pair(comment_type, 0));
        }
        return {total_score_deducted, scores, comments, values, full_scores, comments_sound};
    }
    /**
     * @brief 判断笔画个数是否正确
     * 
     * 
     * @param stroke_info_array 笔画集
     * 
     */
    
    /**
     * @brief
     *
     * @param stroke_order_index_array 由网络端得到的待测字的正确笔顺,例如返回的是0,2,1,表示正确的笔顺是第一划,第三划,第二划
     * @return configor::json
     * */
    std::tuple<configor::json, std::vector<int>> score(
        std::vector<std::string> standard_lines,
        std::vector<std::string> evaluate_lines,
        CharacterInfo char_info,
        std::vector<StructionInfo> struction_info_array,
        std::vector<StrokeInfo> stroke_info_array,
        std::string config_line,
        bool is_character_right

    )
    {
        //如果笔画数目不正确,扣掉部件和笔画分数,只保留整体分数
        //如果笔顺数目不正确,因不影响部件切分,保留部件分和笔画分,扣除笔顺分
        //如果写错字了,目前正常打分,看效果
        m_config.parse_data_1_0(config_line);
        m_standard_segments = load_from_content(standard_lines, m_config);
        m_evaluate_segments = load_from_content(evaluate_lines, m_config);
        get_stroke_map(m_standard_character, m_standard_segments, char_info, struction_info_array, stroke_info_array, true);
        get_stroke_map(m_evaluate_character, m_evaluate_segments, char_info, struction_info_array, stroke_info_array, false);
        auto standard_all_strokes_sorted_by_order = get_all_strokes(m_standard_character);
        std::vector<Stroke> evaluate_all_strokes_sorted_by_order;
        configor::json result;
        if (!m_evaluate_character.m_structions.empty())
        {
            evaluate_all_strokes_sorted_by_order = get_all_strokes(m_evaluate_character);
            std::vector<std::unordered_map<std::string, std::string>> all_strokes_comments;
            std::vector<std::unordered_map<std::string, std::vector<std::string>>> all_strokes_comments_sound;
            std::vector<std::unordered_map<std::string, int>> all_strokes_values;
            std::vector<std::unordered_map<std::string, double>> all_strokes_scores;
            std::vector<std::unordered_map<std::string, double>> all_strokes_full_scores;
            double strokes_deduction_score = 0;
            for (
                auto standard_stroke_iter = standard_all_strokes_sorted_by_order.begin(),
                     evaluate_stroke_iter = evaluate_all_strokes_sorted_by_order.begin();
                standard_stroke_iter != standard_all_strokes_sorted_by_order.end(),
                     evaluate_stroke_iter != evaluate_all_strokes_sorted_by_order.end();
                ++standard_stroke_iter, ++evaluate_stroke_iter)
            {

                auto [stroke_score, stroke_score_items, stroke_comment_items, stroke_value_items, stroke_full_score_items, stroke_comments_sound_items] = score(*standard_stroke_iter, *evaluate_stroke_iter, m_config);
                strokes_deduction_score += stroke_score;

                all_strokes_comments.push_back(stroke_comment_items);
                all_strokes_comments_sound.push_back(stroke_comments_sound_items);
                all_strokes_values.push_back(stroke_value_items);
                all_strokes_scores.push_back(stroke_score_items);
                all_strokes_full_scores.push_back(stroke_full_score_items);
            }
            auto all_strokes_comment_dict = merge_map(all_strokes_comments);
            strokes_deduction_score /= standard_all_strokes_sorted_by_order.size();

            double struction_deduction_score = 0;
            std::vector<double> struction_score_array;
            std::vector<std::unordered_map<std::string, double>> struction_score_items_array;
            std::vector<std::unordered_map<std::string, double>> struction_full_score_items_array;
            std::vector<std::unordered_map<std::string, int>> strution_value_items_array;
            std::vector<std::unordered_map<std::string, std::string>> all_struction_comments;
            std::vector<std::unordered_map<std::string, std::vector<std::string>>> all_struction_comments_sound;
            std::vector<std::unordered_map<std::string, double>> all_struction_double_values;
            auto standard_structions = m_standard_character.m_structions;
            auto evaluate_structions = m_evaluate_character.m_structions;
            for (
                auto standard_struction_iter = standard_structions.begin(),
                     evaluate_struction_iter = evaluate_structions.begin();
                standard_struction_iter != standard_structions.end(),
                     evaluate_struction_iter != evaluate_structions.end();
                ++standard_struction_iter, ++evaluate_struction_iter)
            {
                auto [struction_score, struction_score_items, struction_comment_items, struction_value_items, struction_full_score_items, struction_double_value_items, struction_comment_sound_items] = score(*standard_struction_iter, *evaluate_struction_iter, m_config);
                struction_deduction_score += struction_score;
                all_struction_comments.push_back(struction_comment_items);
                all_struction_comments_sound.push_back(struction_comment_sound_items);
                struction_score_array.push_back(struction_score);
                struction_score_items_array.push_back(struction_score_items);
                struction_full_score_items_array.push_back(struction_full_score_items);
                strution_value_items_array.push_back(struction_value_items);
                all_struction_double_values.push_back(struction_double_value_items);
            }
            struction_deduction_score /= standard_structions.size();
            auto max_iter = std::max_element(struction_score_array.begin(), struction_score_array.end(), [](auto x, auto y)
                                             { return x < y; });

            auto structions_deduction_score = std::accumulate(struction_score_array.begin(), struction_score_array.end(), 0.0);
            auto struction_index = max_iter - struction_score_array.begin();
            auto struction_comment_items = all_struction_comments[struction_index];
            auto struction_comment_sound_items = all_struction_comments_sound[struction_index];
            auto struction_value_items = strution_value_items_array[struction_index];
            std::vector<int> struction_angle_result_array;
            std::transform(strution_value_items_array.begin(), strution_value_items_array.end(), std::back_inserter(struction_angle_result_array), [](auto x)
                           { return x["struction_angle"]; });
            std::vector<double> struction_angle_value_array;
            std::transform(all_struction_double_values.begin(), all_struction_double_values.end(), std::back_inserter(struction_angle_value_array), [](auto x)
                           { return x["struction_angle"]; });
            auto segment_indexes = get_struction_segments_index(evaluate_structions[struction_index]);
            auto [character_deduction_score, character_score_items, character_comment_items, character_value_items, character_full_score_items, character_comment_sound_items] = score(m_standard_character, m_evaluate_character, struction_angle_result_array, struction_angle_value_array, m_config);
            auto [base_deduction_score, base_score_items, base_comment_items, base_value_items, base_full_score_items, base_comment_sound_items] = score_base(
                is_character_right, 
                m_config, 
                stroke_info_array,
                standard_lines,
                evaluate_lines
            );
            auto total_score = 100 * (1 - character_deduction_score - struction_deduction_score - strokes_deduction_score - base_deduction_score);
            auto [out_json, strokes_indexes] = parse_to_old(
                total_score,
                is_character_right,
                base_score_items,
                base_comment_items,
                base_comment_sound_items,
                base_value_items,
                base_full_score_items,
                character_score_items,
                character_comment_items,
                character_comment_sound_items,
                character_value_items,
                character_full_score_items,
                struction_score_items_array,
                struction_comment_items,
                struction_comment_sound_items,
                struction_value_items,
                struction_full_score_items_array,
                all_strokes_scores,
                all_strokes_comments,
                all_strokes_comments_sound,
                all_strokes_values,
                all_strokes_full_scores
            );
                    
           
            int red_component = (int)(m_config.m_data["red_component"].as_float());
            if (red_component==1)
            {
                return {out_json, strokes_indexes};
            }
            else if (red_component==2)
            {
                return {out_json, segment_indexes};
            }
        }
        else
        {
            auto [character_deduction_score, character_score_items, character_comment_items, character_value_items, character_full_score_items, character_comment_sound_items] = score(m_standard_character, m_evaluate_character, {}, {}, m_config);
            auto [base_deduction_score, base_score_items, base_comment_items, base_value_items, base_full_score_items, base_comment_sound_items] = score_base(
                //standard_all_strokes_sorted_by_order, 
                //evaluate_all_strokes_sorted_by_order, 
                
                is_character_right, 
                m_config, 
                stroke_info_array,
                standard_lines,
                evaluate_lines
            );
            auto total_score = 100 * (1 - (character_deduction_score + base_deduction_score) * 2);
            auto [out_json, strokes_indexes] = parse_to_old(
                total_score,
                is_character_right,
                base_score_items,
                base_comment_items,
                base_comment_sound_items,
                base_value_items,
                base_full_score_items,
                character_score_items,
                character_comment_items,
                character_comment_sound_items,
                character_value_items,
                character_full_score_items,
                {},
                std::unordered_map<std::string, std::string>(),
                std::unordered_map<std::string, std::vector<std::string>>(),
                std::unordered_map<std::string, int>(),
                {},
                {},
                {},
                {},
                {},
                {}
            );
            int red_component = (int)(m_config.m_data["red_component"].as_float());
            if (red_component==1)
            {
                return {out_json, strokes_indexes};
            }
            else if (red_component==2)
            {
                return {out_json, std::vector<int>()};
            }
        }
    }

    std::tuple<configor::json, std::vector<int>> parse_to_old(
        double score,
        bool is_character_right,
        std::unordered_map<std::string, double> base_score_items,
        std::unordered_map<std::string, std::string> base_comment_items,
        std::unordered_map<std::string, std::vector<std::string>> base_comment_sound_items,
        std::unordered_map<std::string, int> base_value_items,
        std::unordered_map<std::string, double> base_full_score_items,
        std::unordered_map<std::string, double> character_score_items,
        std::unordered_map<std::string, std::string> character_comment_items,
        std::unordered_map<std::string, std::vector<std::string>> character_comment_sound_items,
        std::unordered_map<std::string, int> character_value_items,
        std::unordered_map<std::string, double> character_full_score_items,
        std::vector<std::unordered_map<std::string, double>> struction_score_items_array,
        std::unordered_map<std::string, std::string> struction_comment_items,
        std::unordered_map<std::string, std::vector<std::string>> struction_comment_sound_items,
        std::unordered_map<std::string, int> struction_value_items,
        std::vector<std::unordered_map<std::string, double>> struction_full_score_items_array,
        std::vector<std::unordered_map<std::string, double>> stroke_score_items_array,
        std::vector<std::unordered_map<std::string, std::string>> stroke_comment_items_array,
        std::vector<std::unordered_map<std::string, std::vector<std::string>>> stroke_comment_sound_items_array,
        std::vector<std::unordered_map<std::string, int>> stroke_value_items_array,
        std::vector<std::unordered_map<std::string, double>> stroke_full_score_items_array)
    {
        auto centerOfGravityType = 0;
        auto character_position_value = character_value_items["character_position"];
        switch (character_position_value)
        {

        case 1:
            centerOfGravityType = 1;
            break;
        case 2:
            centerOfGravityType = 2;
            break;
        case 3:
            centerOfGravityType = 3;
            break;
        case 4:
            centerOfGravityType = 4;
            break;
        case 5:
            centerOfGravityType = 5;
            break;
        case 6:
            centerOfGravityType = 7;
            break;
        case 7:
            centerOfGravityType = 6;
            break;
        case 8:
            centerOfGravityType = 8;
            break;
        }
        auto character_position_full_score = character_full_score_items["character_position"];
        auto character_position_score = character_score_items["character_position"];
        auto centerOfGravityScore = 100;
        if (character_position_full_score != 0)
        {
            centerOfGravityScore = (int)(100 * (character_position_full_score - character_position_score) / character_position_full_score);
        }
        else
        {
            centerOfGravityScore = 100;
        }
        auto character_size_value = character_value_items["character_size"];
        auto character_scale_value = character_value_items["character_scale"];
        auto fontSize = 0;
        switch (character_size_value)
        {
        case 1:
            fontSize = 1;
            break;
        case 2:
            fontSize = 2;
            break;
        }
        if (character_scale_value > 0)
        {
            fontSize = character_scale_value + 10;
        }
        //与原版一致
        if (fontSize == 11)
        {
            fontSize = 3;
        }
        if (fontSize == 12)
        {
            fontSize = 4;
        }
        
        auto character_size_full_score = character_full_score_items["character_size"];
        auto character_scale_full_score = character_full_score_items["character_size"];
        auto character_size_score = character_score_items["character_size"];
        auto character_scale_score = character_score_items["character_scale"];
        auto fontSizeScore = 100;
        auto character_size_score_ = 100;
        auto character_scale_score_ = 100;
        if (character_size_full_score != 0)
        {
            character_size_score_ = 100 * (character_size_full_score - character_size_score) / character_size_full_score;
        }
        else
        {
            character_size_score_ = 100;
        }
        if (character_scale_full_score != 0)
        {
            character_scale_score_ = 100 * (character_scale_full_score - character_scale_score) / character_scale_full_score;
        }
        else
        {
            character_scale_score_ = 100;
        }
        fontSizeScore = (int)(std::min({character_size_score_, character_scale_score_}));

        auto character_angle_value = character_value_items["character_angle"];
        auto fountType = 0;
        switch (character_angle_value)
        {
        case 1:
            fountType = 1;
            break;
        case 2:
            fountType = 3;
            break;
        case 3:
            fountType = 2;
            break;
        }
        auto character_angle_full_score = character_full_score_items["character_angle"];
        auto character_angle_score = character_score_items["character_angle"];
        auto fountScore = 100;
        if (character_angle_full_score != 0)
        {
            fountScore = (int)(100 * (character_angle_full_score - character_angle_score) / character_angle_full_score);
        }
        else
        {
            fountScore = 100;
        }
        auto strokeCount = std::string("");
        std::vector <std::string> z103strokeCountSound;
        auto strokeCountDiff = 0;
        auto strokeCountScore = 100;
        if (base_score_items.find("stroke_count") != base_score_items.end())
        {
            strokeCount = base_comment_items["stroke_count"];
            z103strokeCountSound.insert(
                z103strokeCountSound.end(), 
                base_comment_sound_items["stroke_count"].begin(),
                base_comment_sound_items["stroke_count"].end());
            strokeCountDiff = base_value_items["stroke_count"];
            auto stroke_count_score = base_score_items["stroke_count"];
            auto stroke_count_full_score = base_full_score_items["stroke_count"];
            strokeCountScore = (int)(100 * (stroke_count_full_score - stroke_count_score) / stroke_count_full_score);
        }
        std::vector<std::string> stroke_length_comment_array;
        std::transform(stroke_comment_items_array.begin(), stroke_comment_items_array.end(), std::back_inserter(stroke_length_comment_array), [](auto x)
                       { return x["stroke_size"]; });
        std::vector<std::vector<std::string>> stroke_length_sound_array_group;
        std::transform(stroke_comment_sound_items_array.begin(), stroke_comment_sound_items_array.end(), std::back_inserter(stroke_length_sound_array_group), [](auto x)
                       { return x["stroke_size"]; });
       
        std::vector<double> stroke_length_score_array;
        std::transform(stroke_score_items_array.begin(), stroke_score_items_array.end(), std::back_inserter(stroke_length_score_array), [](auto x)
                       { return x["stroke_size"]; });
        std::vector<double> stroke_length_full_score_array;
        std::transform(stroke_full_score_items_array.begin(), stroke_full_score_items_array.end(), std::back_inserter(stroke_length_full_score_array), [](auto x)
                       { return x["stroke_size"]; });
        std::vector<double> stroke_length_score_array_;
        auto stroke_length_full_score_array_length = stroke_length_full_score_array.size();

        for (auto i = 0; i < stroke_length_full_score_array_length; ++i)
        {
            auto stroke_length_full_score = stroke_full_score_items_array[i]["stroke_size"];
            auto stroke_length_score = stroke_score_items_array[i]["stroke_size"];
            if (stroke_length_full_score != 0)
            {
                stroke_length_score_array_.push_back(100 * (stroke_length_full_score - stroke_length_score) / stroke_length_full_score);
            }
            else
            {
                stroke_length_score_array_.push_back(100);
            }
        }
        
        std::vector<std::tuple<int,double>> stroke_length_score_info_array_without_100;
        for (auto i=0; i<stroke_length_score_array_.size(); ++i)
        {
            auto stroke_length_score = stroke_length_score_array_[i];
            if (stroke_length_score<100)
            {
                stroke_length_score_info_array_without_100.push_back({i, stroke_length_score});
            }
        }
        std::sort(stroke_length_score_info_array_without_100.begin(), stroke_length_score_info_array_without_100.end(), [](auto x, auto y){
            auto [x_index, x_score] = x;
            auto [y_index, y_score] = y;
            return x_score>=y_score;
        });
        auto strokeLengthScore = stroke_length_score_array_.size() != 0 ? (int)(std::accumulate(stroke_length_score_array_.begin(), stroke_length_score_array_.end(), 0.0) / stroke_length_score_array_.size()) : 100;
        int top_strokes_count = (int)(m_config.m_data["top_strokes_count"].as_float());
        std::vector<std::string> stroke_length_comment_array_without_empty_string;
       
        std::vector<std::vector<std::string>> stroke_length_comment_sound_array_without_empty_string_group;
   
        std::vector<int> stroke_red_index_array;
        std::vector<StrokeCommentMessageInfo> stroke_comment_message_info_array;
        for (auto i=0; i<std::min((int)(stroke_length_score_info_array_without_100.size()),(int)top_strokes_count); ++i)
        {
            auto [index, score] = stroke_length_score_info_array_without_100[i];
            stroke_length_comment_array_without_empty_string.push_back(stroke_length_comment_array[index]);
            stroke_length_comment_sound_array_without_empty_string_group.push_back(stroke_length_sound_array_group[index]);
            stroke_red_index_array.push_back(index);
            stroke_comment_message_info_array.push_back({
                stroke_length_comment_array[index],
                stroke_length_sound_array_group[index],
                index
            });
        }

        
        std::vector<std::string> stroke_length_comment_sound_array_without_empty_string;
        for (auto sound : stroke_length_comment_sound_array_without_empty_string_group)
        {
            stroke_length_comment_sound_array_without_empty_string.insert(
                stroke_length_comment_sound_array_without_empty_string.end(), 
                sound.begin(), sound.end());
        }
        auto strokeLength = merge_string_vector(stroke_length_comment_array_without_empty_string, std::string("，"));
        auto z106strokeLengthSound = stroke_length_comment_sound_array_without_empty_string;
        auto strokeOrderScore = 100;
        std::string strokeOrder("");
        std::vector <std::string> z104strokeOrderSound;
        if (base_score_items.find("stroke_order") != base_score_items.end())
        {
            strokeOrder = base_comment_items["stroke_order"];
            z104strokeOrderSound.insert(
                z104strokeOrderSound.end(),
                base_comment_sound_items["stroke_order"].begin(),
                base_comment_sound_items["stroke_order"].end());
            auto stroke_order_score = base_score_items["stroke_order"];
            auto stroke_order_full_score = base_full_score_items["stroke_order"];
            strokeOrderScore = (int)(100 * (stroke_order_full_score - stroke_order_score) / stroke_order_full_score);
        }

        std::vector<std::string> stroke_non_length_comment_array;
        for (auto key : {"stroke_position", "stroke_angle"})
        {
            std::transform(stroke_comment_items_array.begin(), stroke_comment_items_array.end(), std::back_inserter(stroke_non_length_comment_array), [key](auto x)
                           { return x[key]; });
        }
        std::vector<std::vector<std::string>> stroke_non_length_comment_sound_array_group;
        for (auto key : {"stroke_position", "stroke_angle"})
        {
            std::transform(stroke_comment_sound_items_array.begin(), stroke_comment_sound_items_array.end(), std::back_inserter(stroke_non_length_comment_sound_array_group), [key](auto x)
                           { return x[key]; });
        }
       
        
        std::vector<double> stroke_non_length_score_array;
        for (auto key : {"stroke_position", "stroke_angle"})
        {
            std::transform(stroke_score_items_array.begin(), stroke_score_items_array.end(), std::back_inserter(stroke_non_length_score_array), [key](auto x)
                           { return x[key]; });
        }
        std::vector<double> stroke_non_length_full_score_array;
        for (auto key : {"stroke_position", "stroke_angle"})
        {
            std::transform(stroke_full_score_items_array.begin(), stroke_full_score_items_array.end(), std::back_inserter(stroke_non_length_full_score_array), [key](auto x)
                           { return x[key]; });
        }
        std::vector<double> stroke_non_length_score_array_;
        auto stroke_non_length_full_score_array_length = stroke_non_length_full_score_array.size();
        for (auto i = 0; i < stroke_non_length_full_score_array_length; ++i)
        {
            auto stroke_non_length_full_score = stroke_non_length_full_score_array[i];
            auto stroke_non_length_score = stroke_non_length_score_array[i];
            if (stroke_non_length_full_score != 0)
            {
                auto stroke_non_length_score_ = 100 * (stroke_non_length_full_score - stroke_non_length_score) / stroke_non_length_full_score;
                stroke_non_length_score_array_.push_back(stroke_non_length_score_);
            }
            else
            {
                stroke_non_length_score_array_.push_back(100);
            }

           
        }

        auto stroke_non_length_score = stroke_non_length_score_array_.size() != 0 ? (int)(std::accumulate(stroke_non_length_score_array_.begin(), stroke_non_length_score_array_.end(), 0.0) / stroke_non_length_score_array_.size()) : 100;
        auto spacingStructureScore = (int)(centerOfGravityScore*0.3 + fontSizeScore*0.3 + fountScore*0.3 + stroke_non_length_score*0.1);
      
        std::vector<std::string> stroke_non_length_comment_array_without_empty_string;
        std::copy_if(stroke_non_length_comment_array.begin(), stroke_non_length_comment_array.end(), std::back_inserter(stroke_non_length_comment_array_without_empty_string), [](auto x)
                     { return x != ""; });

        
        std::vector<std::vector<std::string>> stroke_non_length_comment_sound_array_group_without_empty;
        std::copy_if(stroke_non_length_comment_sound_array_group.begin(), stroke_non_length_comment_sound_array_group.end(), std::back_inserter(stroke_non_length_comment_sound_array_group_without_empty), [](auto x)
                    { return !(x.empty()); });

           
        std::vector <std::string> character_comments_without_empty_string;
        for (auto character_comment_pair: character_comment_items)
        {
            if (character_comment_pair.second!="")
            {
                character_comments_without_empty_string.push_back(character_comment_pair.second);
            }
        }
        std::vector<std::vector<std::string>> character_comments_sound_group_without_empty_string;
        for (auto character_comment_sound_pair: character_comment_sound_items)
        {
            if (character_comment_sound_pair.second.size()>0)
            {
                
                character_comments_sound_group_without_empty_string.push_back(
                    character_comment_sound_pair.second);
            }
        }
        int display_count = (int)(m_config.m_data["top_structions_count"].as_float());
        std::vector <std::string> spacing_struction_comments(character_comments_without_empty_string);
        if (character_comments_without_empty_string.size()<display_count)
        {
            spacing_struction_comments.insert(spacing_struction_comments.end(), stroke_non_length_comment_array_without_empty_string.begin(),
                stroke_non_length_comment_array_without_empty_string.begin()
                    +std::min(stroke_non_length_comment_array_without_empty_string.size(), 
                        display_count-character_comments_without_empty_string.size()));
        }             
        auto spacingStructure = merge_string_vector(spacing_struction_comments, std::string("，"));

        std::vector<std::vector<std::string>> spacing_struction_comments_sound_group(character_comments_sound_group_without_empty_string);
        if (character_comments_sound_group_without_empty_string.size()<display_count)
        {
            for (auto i=0; i<std::min(stroke_non_length_comment_sound_array_group_without_empty.size(), display_count-character_comments_sound_group_without_empty_string.size()); ++i)
            {

                spacing_struction_comments_sound_group.push_back(
                    stroke_non_length_comment_sound_array_group_without_empty[i]);
            }
            
        }   
        std::vector<std::string> spacing_struction_comments_sound;
        for (auto sounds: spacing_struction_comments_sound_group)
        {
            spacing_struction_comments_sound.insert(
                spacing_struction_comments_sound.end(),
                sounds.begin(), sounds.end());
        }         
        auto z105spacingStructureSound = spacing_struction_comments_sound;
        
        auto z100speedScore = 100;

        double total_struction_full_score = 0.0;
        std::vector struction_keys{"struction_position", "struction_angle", "struction_size", "struction_scale"};
        
        for (auto i = 0; i < struction_full_score_items_array.size(); ++i)
        {

            auto struction_full_score_items = struction_full_score_items_array[i];
            auto struction_score_items = struction_score_items_array[i];
            for (auto key : struction_keys)
            {
                auto struction_score_item = struction_score_items[key];
                auto struction_full_score_item = struction_full_score_items[key];
                auto struction_score_ = 100 * (struction_full_score_item - struction_score_item) / struction_full_score_item;
                total_struction_full_score += struction_score_;
            }
        }
        
        auto z101structionScore = struction_full_score_items_array.size() != 0 ? (int)(total_struction_full_score / struction_full_score_items_array.size() / struction_keys.size()) : 100;
        std::vector<std::string> struction_comments;
        for (auto pair : struction_comment_items)
        {
            if (pair.second != "")
            {
                struction_comments.push_back(pair.second);
            }
        }
        auto z102struction = merge_string_vector(struction_comments, "，");
        std::vector<std::string> struction_comments_sound;
        for (auto pair : struction_comment_sound_items)
        {
            if (pair.second.size()!= 0)
            {
                
                struction_comments_sound.insert(struction_comments_sound.end(),
                    pair.second.begin(), pair.second.end());
            }
        }
        auto z107structionSound = struction_comments_sound;

        auto res_score = (int)score;
        auto error = 0;
        
        if (!is_character_right)
        {
            error = 7001;
        }
        else if (strokeOrderScore < 100)
        {
            error = 3;
        }
        if (strokeCountDiff!=0)
        {
            error = 2; //笔画数错误优先于笔顺错误
        }
        std::vector<std::string> z108incorrectCharacterSound;
        if (!is_character_right) {
            z108incorrectCharacterSound = base_comment_sound_items["incorrect_character"];
        }
        configor::json res;
        res["centerOfGravityType"] = centerOfGravityType;
        res["centerOfGravityScore"] = centerOfGravityScore;
        res["error"] = error;
        res["fontSize"] = fontSize;
        res["fontSizeScore"] = fontSizeScore;
        res["fountScore"] = fountScore;
        res["fountType"] = fountType;
        res["score"] = res_score;
        res["spacingStructure"] = spacingStructure==""?"正确":spacingStructure;
        res["spacingStructureScore"] = spacingStructureScore;
        res["status"] = true;
        res["strokeCount"] = strokeCount==""?"正确":strokeCount;
        res["strokeCountDiff"] = strokeCountDiff;
        res["strokeCountScore"] = strokeCountScore;
        res["strokeLength"] = strokeLength==""?"正确":strokeLength;
        res["strokeLengthScore"] = strokeLengthScore;
        res["strokeOrder"] = strokeOrder==""?"正确":strokeOrder;
        res["strokeOrderScore"] = strokeOrderScore;
        res["z100speedScore"] = z100speedScore;
        res["z102struction"] = z102struction==""?"正确":z102struction;
        res["z101structionScore"] = z101structionScore;
        res["z103strokeCountSound"] = z103strokeCountSound;
        res["z104strokeOrderSound"] = z104strokeOrderSound;
        res["z105spacingStructureSound"] = z105spacingStructureSound;
        res["z106strokeLengthSound"] = z106strokeLengthSound;
        res["z107structionSound"] = z107structionSound;
        res["z108incorrectCharacterSound"] = z108incorrectCharacterSound;
        return std::make_tuple(res, stroke_red_index_array);
    }
    std::tuple<configor::json, std::vector<int>> default_old_result()
    {
        configor::json res;
        res["centerOfGravityType"] = 0;
        res["centerOfGravityScore"] = 0;
        res["error"] = 0;
        res["fontSize"] = 0;
        res["fontSizeScore"] = 0;
        res["fountScore"] = 0;
        res["fountType"] = 0;
        res["score"] = 0;
        res["spacingStructure"] = "";
        res["spacingStructureScore"] = 0;
        res["status"] = true;
        res["strokeCount"] = "";
        res["strokeCountDiff"] = 0;
        res["strokeCountScore"] = 0;
        res["strokeLength"] = "";
        res["strokeLengthScore"] = 0;
        res["strokeOrder"] = "";
        res["strokeOrderScore"] = 0;
        res["z100speedScore"] = 0;
        res["z102struction"] = "";
        res["z101structionScore"] = 0;
        res["z103strokeCountSound"] = std::vector<std::string>();
        res["z104strokeOrderSound"] = std::vector<std::string>();
        res["z105spacingStructureSound"] = std::vector<std::string>();
        res["z106strokeLengthSound"] = std::vector<std::string>();
        res["z107structionSound"] = std::vector<std::string>();
        res["z108incorrectCharacterSound"] = std::vector<std::string>();
        return std::make_tuple(res, std::vector<int>());
    }

    double get_real_deduction(int diff_x, int width, int diff_y, int height)
    {
        if (height * width == 0)
        {
            throw ZeroException();
        }
        auto diff_value = std::max({abs((double)diff_x / width), abs((double)diff_y / height)});
        auto diff_value_integer_5x = std::max({abs(5 * diff_x / width), abs(5 * diff_y / height)});

        auto deduction = 0.0;
        switch (diff_value_integer_5x)
        {
        case 0:
        {
            //小于1/5不扣分
            deduction = 0.0;
            break;
        }
        case 1:
        {
            deduction = (diff_value - 0.2) * 0.5; // 0.4
            break;
        }
        case 2:
        {
            deduction = (diff_value - 0.4) * 1.0 + 0.1; // 0.6
            break;
        }
        case 3:
        {
            deduction = (diff_value - 0.6) * 1.5 + 0.3; // 0.8
            break;
        }
        case 4:
        {
            deduction = (diff_value - 0.8) * 2.0 + 0.6; // 1.0
            break;
        }
        }
        return 1 - deduction;
    }
    double score(
        std::vector<std::string> standard_lines,
        std::vector<std::string> evaluate_lines,
        CharacterInfo char_info,
        std::vector<StructionInfo> struction_info_array,
        std::vector<StrokeInfo> stroke_info_array,
        std::string config_line

    )
    {
        //一.求凸包得分
        //位移最大扣20分
        //凸包重叠面积/凸包最大面积
        m_config.parse_data_1_0(config_line);
        m_standard_segments = load_from_content(standard_lines, m_config);
        m_evaluate_segments = load_from_content(evaluate_lines, m_config);
        m_standard_character.m_segments = m_standard_segments;
        m_evaluate_character.m_segments = m_evaluate_segments;
        auto character_width = m_config.m_data["character"]["width"].as_integer();
        auto character_height = m_config.m_data["character"]["height"].as_integer();
        auto standard_mat = m_standard_character.draw(character_width, character_height);
        auto evaluate_mat = m_evaluate_character.draw(character_width, character_height);
        ConvexHull standard_convexhull = ConvexHull(standard_mat);
        ConvexHull evaluate_convexhull = ConvexHull(evaluate_mat);
        auto standard_center = standard_convexhull.get_center();
        auto evaluate_center = evaluate_convexhull.get_center();
        //凸包中心对齐
        // 1.图片放大2倍
        cv::Mat standard_extend_mat = cv::Mat::zeros(2 * character_width, 2 * character_height, 2 * m_standard_character.draw(character_width, character_height).type());
        cv::Mat evaluate_extend_mat = cv::Mat::zeros(2 * character_width, 2 * character_height, 2 * m_standard_character.draw(character_width, character_height).type());
        cv::Rect roi(character_width / 2, character_height / 2, character_width, character_height);
        auto standard_convexhull_mat = standard_convexhull.draw();
        auto evaluate_convexhull_mat = evaluate_convexhull.draw();
        standard_convexhull_mat.copyTo(standard_extend_mat(roi));
        evaluate_convexhull_mat.copyTo(evaluate_extend_mat(roi));
        // 2.中心对齐
        auto diff_center = standard_center - evaluate_center;
        cv::Mat evaluate_convexhull_mat_translated(2 * character_width, 2 * character_height, 2 * m_standard_character.draw(character_width, character_height).type());
        cv::Mat M = cv::Mat::zeros(2, 3, CV_32FC1);
        M.at<float>(0, 0) = 1;
        M.at<float>(0, 2) = diff_center.x;
        M.at<float>(1, 1) = 1;
        M.at<float>(1, 2) = diff_center.y;

        // cv::imshow("dd1", evaluate_extend_mat);
        // cv::waitKey(3000);
        cv::warpAffine(
            evaluate_extend_mat,
            evaluate_convexhull_mat_translated,
            M,
            cv::Size(2 * character_width, 2 * character_height));

        cv::Mat convexhull_intersection = standard_extend_mat & evaluate_convexhull_mat_translated;
        cv::Mat convexhull_union = standard_extend_mat | evaluate_convexhull_mat_translated;
        auto convexhull_score = cv::sum(convexhull_intersection)[0] / cv::sum(convexhull_union)[0];

        // 3.求evaluate_extend_mat的面积,并放大到与standard_extend_mat一致
        auto area_standard_convexhull = cv::sum(standard_extend_mat);
        auto area_evaluate_convexhull = cv::sum(evaluate_convexhull_mat_translated);
        auto area_ratio = area_standard_convexhull[0] / area_evaluate_convexhull[0];
        auto length_ratio = sqrt(area_ratio);
        // cv::Mat M_translate = cv::Mat::zeros(3, 3, CV_32FC1);
        // M_translate.at<float>(0,0) = 1;
        // M_translate.at<float>(0,2) = -(standard_center.x+character_width/2);
        // M_translate.at<float>(1,1) = 1;
        // M_translate.at<float>(1,2) = -(standard_center.y+character_height/2);
        // M_translate.at<float>(2,2) = 1;
        cv::Mat M_resize = cv::Mat::zeros(3, 3, CV_32FC1);
        M_resize.at<float>(0, 0) = length_ratio;
        M_resize.at<float>(0, 2) = 0;
        M_resize.at<float>(1, 1) = length_ratio;
        M_resize.at<float>(1, 2) = 0;
        M_resize.at<float>(2, 2) = 1;
        cv::Mat M_translate_inv = cv::Mat::zeros(3, 3, CV_32FC1);
        M_translate_inv.at<float>(0, 0) = 1;
        M_translate_inv.at<float>(0, 2) = -(standard_center.x + character_width / 2) * (length_ratio - 1);
        M_translate_inv.at<float>(1, 1) = 1;
        M_translate_inv.at<float>(1, 2) = -(standard_center.y + character_height / 2) * (length_ratio - 1);
        M_translate_inv.at<float>(2, 2) = 1;
        cv::Mat M_union = M_translate_inv * M_resize;
        cv::Mat M_union_sub = M_union(cv::Rect(0, 0, 3, 2));
        // M_resize.at<float>(0,0) = length_ratio;
        // M_resize.at<float>(0,2) = (standard_center.x+character_width/2)*(length_ratio-1);
        // M_resize.at<float>(1,1) = length_ratio;
        // M_resize.at<float>(1,2) = (standard_center.y+character_height/2)*(length_ratio-1);
        cv::Mat evaluate_convexhull_resized(standard_extend_mat.size().width, standard_extend_mat.size().height, standard_extend_mat.type());

        // cv::imshow("dd1", evaluate_convexhull_mat_translated);
        // cv::waitKey(3000);

        cv::warpAffine(
            evaluate_convexhull_mat_translated,
            evaluate_convexhull_resized,
            M_union_sub,
            cv::Size(2 * character_width, 2 * character_height));

        // cv::imshow("dd2", evaluate_convexhull_resized);
        // cv::waitKey(3000);

        cv::Mat convexhull_intersection_resized = standard_extend_mat & evaluate_convexhull_resized;
        cv::Mat convexhull_union_resized = standard_extend_mat | evaluate_convexhull_resized;
        if (cv::sum(convexhull_union_resized)[0] == 0)
        {
            throw ZeroException();
        }
        auto convexhull_score_resized = cv::sum(convexhull_intersection_resized)[0] / cv::sum(convexhull_union_resized)[0];
        //把evaluate_convexhull_mat_translated放大
        // cv::imshow("dd",evaluate_convexhull_mat_translated);
        // cv::waitKey(3000);
        // cv::imshow("standard", m_standard_character.draw(character_width, character_height));
        // cv::waitKey(10000);
        // cv::imshow("evaluate", m_evaluate_character.draw(character_width, character_height));
        // cv::waitKey(10000);
        auto scale_score = get_real_deduction(diff_center.x, character_width / 2, diff_center.y, character_height / 2);
        //扣结构分:根据配置文件
        get_stroke_map(m_standard_character, m_standard_segments, char_info, struction_info_array, stroke_info_array, true);
        get_stroke_map(m_evaluate_character, m_evaluate_segments, char_info, struction_info_array, stroke_info_array, false);
        auto is_struction = m_config.m_data["is_struction"].as_bool();
        auto is_stroke_reliable = m_config.m_data["is_stroke_reliable"].as_bool();
        auto stroke_score = 0.0;
        if (is_stroke_reliable)
        {
            std::vector<std::string> angle_name_array{"横", "横钩", "竖", "竖钩", "弯钩", "竖提", "捺", "斜钩", "撇", "提"};
            //把最大的笔画夹角计入扣分rra
            auto standard_stroke_array = m_standard_character.m_strokes;
            auto evaluate_stroke_array = m_evaluate_character.m_strokes;
            if (standard_stroke_array.size() == evaluate_stroke_array.size())
            {
                std::vector<double> angle_diff_array;
                for (auto standard_stroke_iter = standard_stroke_array.begin(),
                          evaluate_stroke_iter = evaluate_stroke_array.begin();
                     standard_stroke_iter != standard_stroke_array.end(),
                          evaluate_stroke_iter != evaluate_stroke_array.end();
                     ++standard_stroke_iter, ++evaluate_stroke_iter)
                {
                    if (std::find(angle_name_array.begin(), angle_name_array.end(), evaluate_stroke_iter->name) == angle_name_array.end())
                    {
                        continue;
                    }

                    if (evaluate_stroke_iter->is_reliable)
                    {
                        auto standard_mat = standard_stroke_iter->draw(character_width, character_height);
                        auto evaluate_mat = evaluate_stroke_iter->draw(character_width, character_height);
                        auto angle_info = get_angle_info_half(standard_mat, evaluate_mat);
                        auto angle = angle_info.diff_half_angle;
                        if (angle_info.diff_half_angle > M_PI)
                        {
                            angle = angle_info.diff_half_angle - 2 * M_PI;
                        }
                        angle_diff_array.push_back(abs(angle));
                    }
                }
                auto max_angle_diff_iter = std::max_element(angle_diff_array.begin(), angle_diff_array.end());
                if (max_angle_diff_iter == angle_diff_array.end())
                {
                    stroke_score = 1.0;
                }
                else
                {
                    auto max_angle_diff = *max_angle_diff_iter;
                    stroke_score = 1 - max_angle_diff / M_PI;
                }
            }
            else
            {
                stroke_score = 1.0;
            }
        }
        auto total_score = 0.0;
        auto warp_score = char_info.warp_score;
        if (warp_score > 0.5)
        {
            warp_score = 0.5;
        }
        if (m_standard_character.type == " " || (m_standard_character.type != " " && !is_struction))
        {
            if (is_stroke_reliable)
            {
                // total_score = ((1-(1-(convexhull_score*0.2+convexhull_score_resized*0.8))*1.7)*0.8+scale_score*0.2-(1-stroke_score)*0.4)+0.1-warp_score*0.2;
                total_score = ((1 - (1 - (convexhull_score * 0.2 + convexhull_score_resized * 0.8)) * 1.7) * 0.8 + scale_score * 0.2 - (1 - stroke_score) * 0.4) + 0.1;
            }
            else
            {
                // total_score = ((1-(1-(convexhull_score*0.7+convexhull_score_resized*0.3))*1.7)*0.8+scale_score*0.2)+0.05-warp_score*0.2;
                total_score = ((1 - (1 - (convexhull_score * 0.7 + convexhull_score_resized * 0.3)) * 1.7) * 0.8 + scale_score * 0.2) + 0.05;
            }
        }
        else if (m_standard_character.type != " " && is_struction)
        {
            auto all_struction_score = 0.0;
            if (m_evaluate_character.m_structions.empty() || m_evaluate_character.m_structions.size() != m_standard_character.m_structions.size())
            {
                all_struction_score = 0.0;
            }
            else
            {
                auto standard_structions = m_standard_character.m_structions;
                auto evaluate_structions = m_evaluate_character.m_structions;
                for (
                    auto standard_struction_iter = standard_structions.begin(),
                         evaluate_struction_iter = evaluate_structions.begin();
                    standard_struction_iter != standard_structions.end(),
                         evaluate_struction_iter != evaluate_structions.end();
                    ++standard_struction_iter, ++evaluate_struction_iter)
                {
                    auto standard_struction_mat = standard_struction_iter->draw(character_width, character_height);
                    auto evaluate_struction_mat = evaluate_struction_iter->draw(character_width, character_height);

                    ConvexHull standard_convexhull = ConvexHull(standard_struction_mat);
                    ConvexHull evaluate_convexhull = ConvexHull(evaluate_struction_mat);
                    auto standard_center = standard_convexhull.get_center();
                    auto evaluate_center = evaluate_convexhull.get_center();

                    cv::Mat standard_extend_mat = cv::Mat::zeros(2 * character_width, 2 * character_height, 2 * m_standard_character.draw(character_width, character_height).type());
                    cv::Mat evaluate_extend_mat = cv::Mat::zeros(2 * character_width, 2 * character_height, 2 * m_standard_character.draw(character_width, character_height).type());
                    cv::Rect roi(character_width / 2, character_height / 2, character_width, character_height);
                    auto standard_convexhull_mat = standard_convexhull.draw();
                    auto evaluate_convexhull_mat = evaluate_convexhull.draw();
                    standard_convexhull_mat.copyTo(standard_extend_mat(roi));
                    evaluate_convexhull_mat.copyTo(evaluate_extend_mat(roi));

                    auto diff_center = standard_center - evaluate_center;
                    cv::Mat evaluate_convexhull_mat_translated(2 * character_width, 2 * character_height, 2 * m_standard_character.draw(character_width, character_height).type());
                    cv::Mat M = cv::Mat::zeros(2, 3, CV_32FC1);
                    M.at<float>(0, 0) = 1;
                    M.at<float>(0, 2) = diff_center.x;
                    M.at<float>(1, 1) = 1;
                    M.at<float>(1, 2) = diff_center.y;

                    cv::warpAffine(
                        evaluate_extend_mat,
                        evaluate_convexhull_mat_translated,
                        M,
                        cv::Size(2 * character_width, 2 * character_height));

                    cv::Mat convexhull_intersection = standard_extend_mat & evaluate_convexhull_mat_translated;
                    cv::Mat convexhull_union = standard_extend_mat | evaluate_convexhull_mat_translated;
                    auto struction_score = cv::sum(convexhull_intersection)[0] / cv::sum(convexhull_union)[0];
                    all_struction_score += struction_score;
                }
                all_struction_score /= standard_structions.size();
            }
            total_score = ((1 - (1 - (convexhull_score * 0.57 + convexhull_score_resized * 0.37 + all_struction_score * 0.06)) * 1.5) * 0.8 + scale_score * 0.2) + 0.05;
        }
        if (total_score < 0.3)
        {
            total_score = 0.3;
        }
        if (total_score > 1.0)
        {
            total_score = 1.0;
        }
        return total_score;

        // auto scale_score = (1-std::max({abs((double)diff_center.x/character_width/2), abs((double)diff_center.y/character_height/2)}));
    }

protected:
    std::vector<Segment> m_standard_segments; //从dot里读取到的原始segment
    std::vector<Segment> m_evaluate_segments; //从dot里读取到的原始segment
    Character m_evaluate_character;
    Character m_standard_character;
    Dot dot;
    Config m_config; //扣分的配置
};
#endif