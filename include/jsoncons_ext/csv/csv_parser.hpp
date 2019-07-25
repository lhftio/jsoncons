// Copyright 2015 Daniel Parker
// Distributed under the Boost license, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// See https://github.com/danielaparker/jsoncons for latest version

#ifndef JSONCONS_CSV_CSV_PARSER_HPP
#define JSONCONS_CSV_CSV_PARSER_HPP

#include <memory> // std::allocator
#include <string>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <system_error>
#include <cctype>
#include <jsoncons/json_exception.hpp>
#include <jsoncons/json_content_handler.hpp>
#include <jsoncons/json_reader.hpp>
#include <jsoncons/json_filter.hpp>
#include <jsoncons/json.hpp>
#include <jsoncons/detail/parse_number.hpp>
#include <jsoncons_ext/csv/csv_error.hpp>
#include <jsoncons_ext/csv/csv_options.hpp>

namespace jsoncons { namespace csv {

enum class csv_mode 
{
    initial,
    header,
    data,
    subfields
};

enum class csv_parse_state 
{
    start,
    cr, 
    column_labels,
    expect_comment_or_record,
    expect_record,
    end_record,
    comment,
    between_fields,
    quoted_string,
    unquoted_string,
    before_unquoted_string,
    escaped_value,
    minus, 
    zero,  
    integer,
    fraction,
    exp1,
    exp2,
    exp3,
    done
};

struct default_csv_parsing
{
    bool operator()(csv_errc, const ser_context&) noexcept
    {
        return false;
    }
};

namespace detail {

    template <class CharT, class Allocator>
    class m_columns_filter : public basic_json_content_handler<CharT>
    {
    public:
        typedef typename basic_json_content_handler<CharT>::string_view_type string_view_type;
        typedef CharT char_type;
        typedef Allocator allocator_type;
        typedef typename basic_json_options<CharT>::string_type string_type;
        typedef typename std::allocator_traits<allocator_type>:: template rebind_alloc<string_type> string_allocator_type;
    private:
        typedef basic_json<CharT, preserve_order_policy, Allocator> json_type;

        basic_json_content_handler<CharT>& handler_;
        std::vector<string_type, string_allocator_type> column_names_;
        std::vector<json_decoder<json_type>> decoders_;
        size_t column_index_;
        int level_;
    public:
        m_columns_filter() = delete;

        m_columns_filter(basic_json_content_handler<CharT>& handler)
            : handler_(handler), column_index_(0), level_(0)
        {
        }

        void initialize(const std::vector<string_type, string_allocator_type>& column_names)
        {
            null_ser_context context;
            for (const auto& name : column_names)
            {
                column_names_.push_back(name);
                decoders_.push_back(json_decoder<json_type>());
                decoders_.back().begin_array(semantic_tag::none, context);
            }
            column_index_ = 0;
            level_ = 0;
        }

        void skip_column()
        {
            ++column_index_;
        }

        void do_flush() override
        {
            null_ser_context context;
            handler_.begin_object(semantic_tag::none, context);
            for (size_t i = 0; i < column_names_.size(); ++i)
            {
                handler_.name(column_names_[i], context);
                decoders_[i].end_array(context);
                decoders_[i].flush();
                auto j = decoders_[i].get_result();
                j.dump(handler_);
            }
            handler_.end_object(context);
            handler_.flush();
        }

        bool do_begin_object(semantic_tag, const ser_context&) override
        {
            JSONCONS_THROW(json_runtime_error<std::invalid_argument>("unexpected begin_object"));
        }

        bool do_end_object(const ser_context&) override
        {
            JSONCONS_THROW(json_runtime_error<std::invalid_argument>("unexpected end_object"));
        }

        bool do_begin_array(semantic_tag tag, const ser_context& context) override
        {
            if (column_index_ < column_names_.size())
            {
                decoders_[column_index_].begin_array(tag, context);
                ++level_;
            }
            return true;
        }

        bool do_end_array(const ser_context& context) override
        {
            if (level_ > 0)
            {
                decoders_[column_index_].end_array(context);
                ++column_index_;
                --level_;
            }
            else
            {
                column_index_ = 0;
            }
            return true;
        }

        bool do_name(const string_view_type&, const ser_context&) override
        {
            JSONCONS_THROW(json_runtime_error<std::invalid_argument>("unexpected name"));
        }

        bool do_null_value(semantic_tag tag, const ser_context& context) override
        {
            if (column_index_ < column_names_.size())
            {
                decoders_[column_index_].null_value(tag, context);
                if (level_ == 0)
                {
                    ++column_index_;
                }
            }
            return true;
        }

        bool do_string_value(const string_view_type& value, semantic_tag tag, const ser_context& context) override
        {
            if (column_index_ < column_names_.size())
            {
                decoders_[column_index_].string_value(value, tag, context);
                if (level_ == 0)
                {
                    ++column_index_;
                }
            }
            return true;
        }

        bool do_byte_string_value(const byte_string_view& value,
                                  semantic_tag tag,
                                  const ser_context& context) override
        {
            if (column_index_ < column_names_.size())
            {
                decoders_[column_index_].byte_string_value(value, tag, context);
                if (level_ == 0)
                {
                    ++column_index_;
                }
            }
            return true;
        }

        bool do_double_value(double value,
                             semantic_tag tag,
                             const ser_context& context) override
        {
            if (column_index_ < column_names_.size())
            {
                decoders_[column_index_].double_value(value, tag, context);
                if (level_ == 0)
                {
                    ++column_index_;
                }
            }
            return true;
        }

        bool do_int64_value(int64_t value,
                            semantic_tag tag,
                            const ser_context& context) override
        {
            if (column_index_ < column_names_.size())
            {
                decoders_[column_index_].int64_value(value, tag, context);
                if (level_ == 0)
                {
                    ++column_index_;
                }
            }
            return true;
        }

        bool do_uint64_value(uint64_t value,
                             semantic_tag tag,
                             const ser_context& context) override
        {
            if (column_index_ < column_names_.size())
            {
                decoders_[column_index_].uint64_value(value, tag, context);
                if (level_ == 0)
                {
                    ++column_index_;
                }
            }
            return true;
        }

        bool do_bool_value(bool value, semantic_tag tag, const ser_context& context) override
        {
            if (column_index_ < column_names_.size())
            {
                decoders_[column_index_].bool_value(value, tag, context);
                if (level_ == 0)
                {
                    ++column_index_;
                }
            }
            return true;
        }
    };

} // namespace detail

template<class CharT,class Allocator=std::allocator<CharT>>
class basic_csv_parser : public ser_context
{
    typedef basic_string_view<CharT> string_view_type;
    typedef CharT char_type;
    typedef Allocator allocator_type;
    typedef typename std::allocator_traits<allocator_type>:: template rebind_alloc<CharT> char_allocator_type;
    typedef std::basic_string<CharT,std::char_traits<CharT>,char_allocator_type> string_type;
    typedef typename std::allocator_traits<allocator_type>:: template rebind_alloc<string_type> string_allocator_type;
    typedef typename std::allocator_traits<allocator_type>:: template rebind_alloc<csv_mode> csv_mode_allocator_type;
    typedef typename std::allocator_traits<allocator_type>:: template rebind_alloc<csv_type_info> csv_type_info_allocator_type;
    typedef typename std::allocator_traits<allocator_type>:: template rebind_alloc<std::vector<string_type,string_allocator_type>> string_vector_allocator_type;
    typedef typename std::allocator_traits<allocator_type>:: template rebind_alloc<csv_parse_state> csv_parse_state_allocator_type;
    typedef basic_json<CharT,preserve_order_policy,Allocator> json_type;

    static const int default_depth = 3;

    csv_parse_state state_;
    detail::m_columns_filter<CharT,Allocator> m_columns_filter_;
    std::vector<csv_mode,csv_mode_allocator_type> stack_;
    basic_json_content_handler<CharT>& handler_;
    std::function<bool(csv_errc,const ser_context&)> err_handler_;
    unsigned long column_;
    unsigned long line_;
    string_type value_buffer_;
    int depth_;
    const basic_csv_decode_options<CharT>& options_;
    std::vector<string_type,string_allocator_type> column_names_;
    std::vector<std::vector<string_type,string_allocator_type>,string_vector_allocator_type> column_values_;
    std::vector<csv_type_info,csv_type_info_allocator_type> column_types_;
    std::vector<string_type,string_allocator_type> column_defaults_;
    size_t column_index_;
    size_t level_;
    size_t offset_;
    jsoncons::detail::string_to_double to_double_; 
    const CharT* begin_input_;
    const CharT* input_end_;
    const CharT* input_ptr_;
    bool continue_;
    std::vector<csv_parse_state,csv_parse_state_allocator_type> state_stack_;

public:
    basic_csv_parser(basic_json_content_handler<CharT>& handler)
       : basic_csv_parser(handler, 
                          basic_csv_options<CharT>::get_default_options(), 
                          default_csv_parsing())
    {
    }

    basic_csv_parser(basic_json_content_handler<CharT>& handler,
                     const basic_csv_decode_options<CharT>& options)
        : basic_csv_parser(handler, 
                           options, 
                           default_csv_parsing())
    {
    }

    basic_csv_parser(basic_json_content_handler<CharT>& handler,
                     std::function<bool(csv_errc,const ser_context&)> err_handler)
        : basic_csv_parser(handler, basic_csv_options<CharT>::get_default_options(), err_handler)
    {
    }

    basic_csv_parser(basic_json_content_handler<CharT>& handler,
                     const basic_csv_decode_options<CharT>& options,
                     std::function<bool(csv_errc,const ser_context&)> err_handler)
       : m_columns_filter_(handler),
         handler_(options.mapping() == mapping_type::m_columns ? m_columns_filter_ : handler),
         err_handler_(err_handler),
         options_(options),
         level_(0),
         offset_(0),
         begin_input_(nullptr),
         input_end_(nullptr),
         input_ptr_(nullptr),
         continue_(true)
         
    {
        depth_ = default_depth;
        state_ = csv_parse_state::start;
        line_ = 1;
        column_ = 1;
        column_index_ = 0;
        stack_.reserve(default_depth);
        reset();
    }

    ~basic_csv_parser()
    {
    }

    bool done() const
    {
        return state_ == csv_parse_state::done;
    }

    bool stopped() const
    {
        return !continue_;
    }

    bool source_exhausted() const
    {
        return input_ptr_ == input_end_;
    }

    const std::vector<std::basic_string<CharT>>& column_labels() const
    {
        return column_names_;
    }

    void reset()
    {
        stack_.clear();
        column_names_.clear();
        column_types_.clear();
        column_defaults_.clear();

        stack_.push_back(csv_mode::initial);

        for (auto name : options_.column_names())
        {
            column_names_.emplace_back(name.data(),name.size());
        }
        for (auto name : options_.column_types())
        {
            column_types_.push_back(name);
        }
        for (auto name : options_.column_defaults())
        {
            column_defaults_.emplace_back(name.data(), name.size());
        }
        if (options_.header_lines() > 0)
        {
            stack_.push_back(csv_mode::header);
        }
        else
        {
            stack_.push_back(csv_mode::data);
        }
        state_ = csv_parse_state::start;
        column_index_ = 0;
        column_ = 1;
        level_ = 0;
    }

    void restart()
    {
        continue_ = true;
    }

    void parse_some()
    {
        std::error_code ec;
        parse_some(ec);
        if (ec)
        {
            throw ser_error(ec,line_,column_);
        }
    }

    void parse_some(std::error_code& ec)
    {
        const CharT* local_input_end = input_end_;

        if (input_ptr_ == local_input_end && continue_)
        {
            switch (state_)
            {
                case csv_parse_state::before_unquoted_string: 
                    value_buffer_.clear();
                    JSONCONS_FALLTHROUGH;
                case csv_parse_state::unquoted_string: 
                    if (options_.trim_leading() || options_.trim_trailing())
                    {
                        trim_string_buffer(options_.trim_leading(),options_.trim_trailing());
                    }
                    if (!options_.ignore_empty_lines() || (column_index_ > 0 || value_buffer_.length() > 0))
                    {
                        if (stack_.back() != csv_mode::subfields)
                        {
                            before_field();
                        }
                        end_unquoted_string_value();
                        after_field();
                    }
                    break;
                case csv_parse_state::escaped_value:
                    if (options_.quote_escape_char() == options_.quote_char())
                    {
                        if (stack_.back() != csv_mode::subfields)
                        {
                            before_field();
                        }
                        end_quoted_string_value(ec);
                        if (ec) return;
                        after_field();
                    }
                    break;
                default:
                    break;
            }
            if (column_index_ > 0)
            {
                after_record();
            }
            switch (stack_.back()) 
            {
                case csv_mode::header:
                    stack_.pop_back();
                    break;
                case csv_mode::data:
                    stack_.pop_back();
                    break;
                default:
                    break;
            }
            continue_ = handler_.end_array(*this);
            if (!(stack_.size() == 1 && stack_.back() == csv_mode::initial))
            {
                err_handler_(csv_errc::unexpected_eof, *this);
                ec = csv_errc::unexpected_eof;
                continue_ = false;
                return;
            }
            stack_.pop_back();

            handler_.flush();
            continue_ = false;
        }

        for (; (input_ptr_ < local_input_end) && continue_;)
        {
            CharT curr_char = *input_ptr_;

            switch (state_) 
            {
                case csv_parse_state::cr:
                    ++line_;
                    column_ = 1;
                    switch (*input_ptr_)
                    {
                        case '\n':
                            ++input_ptr_;
                            state_ = pop_state();
                            break;
                        default:
                            state_ = pop_state();
                            break;
                    }
                    break;
                case csv_parse_state::start:
                    if (options_.mapping() != mapping_type::m_columns)
                    {
                        continue_ = handler_.begin_array(semantic_tag::none, *this);
                    }
                    if (!options_.assume_header() && options_.mapping() == mapping_type::n_rows && options_.column_names().size() > 0)
                    {
                        column_index_ = 0;
                        state_ = csv_parse_state::column_labels;
                        continue_ = handler_.begin_array(semantic_tag::none, *this);
                    }
                    else
                    {
                        state_ = csv_parse_state::expect_comment_or_record;
                    }
                    break;
                case csv_parse_state::column_labels:
                    if (column_index_ < column_names_.size())
                    {
                        continue_ = handler_.string_value(column_names_[column_index_], semantic_tag::none, *this);
                        ++column_index_;
                    }
                    else
                    {
                        continue_ = handler_.end_array(*this);
                        state_ = csv_parse_state::expect_comment_or_record; 
                        //stack_.back() = csv_mode::data;
                        column_index_ = 0;
                    }
                    break;
                case csv_parse_state::comment: 
                    switch (curr_char)
                    {
                        case '\n':
                        {
                            ++line_;
                            column_ = 1;
                            state_ = csv_parse_state::expect_comment_or_record;
                            break;
                        }
                        case '\r':
                            ++line_;
                            column_ = 1;
                            state_ = csv_parse_state::expect_comment_or_record;
                            push_state(state_);
                            state_ = csv_parse_state::cr;
                            break;
                        default:
                            ++column_;
                            break;
                    }
                    ++input_ptr_;
                    break;
                
                case csv_parse_state::expect_comment_or_record:
                    value_buffer_.clear();
                    if (curr_char == options_.comment_starter())
                    {
                        state_ = csv_parse_state::comment;
                        ++column_;
                        ++input_ptr_;
                    }
                    else
                    {
                        state_ = csv_parse_state::expect_record;
                    }
                    break;
                case csv_parse_state::escaped_value: 
                    {
                        if (curr_char == options_.quote_char())
                        {
                            value_buffer_.push_back(static_cast<CharT>(curr_char));
                            state_ = csv_parse_state::quoted_string;
                            ++column_;
                            ++input_ptr_;
                        }
                        else if (options_.quote_escape_char() == options_.quote_char())
                        {
                            state_ = csv_parse_state::between_fields;
                        }
                        else
                        {
                            ++column_;
                            ++input_ptr_;
                        }
                    }
                    break;
                case csv_parse_state::quoted_string: 
                    {
                        if (curr_char == options_.quote_escape_char())
                        {
                            state_ = csv_parse_state::escaped_value;
                        }
                        else if (curr_char == options_.quote_char())
                        {
                            state_ = csv_parse_state::between_fields;
                        }
                        else
                        {
                            value_buffer_.push_back(static_cast<CharT>(curr_char));
                        }
                    }
                    ++column_;
                    ++input_ptr_;
                    break;
                case csv_parse_state::between_fields:
                    switch (curr_char)
                    {
                        case '\r':
                        case '\n':
                        {
                            after_newline_between_fields(ec);
                            if (ec)
                            {
                                return;
                            }
                            state_ = csv_parse_state::end_record;
                            break;
                        }
                        default:
                            if (curr_char == options_.field_delimiter() || (options_.subfield_delimiter().second && curr_char == options_.subfield_delimiter().first))
                            {
                                if (options_.trim_leading() || options_.trim_trailing())
                                {
                                    trim_string_buffer(options_.trim_leading(),options_.trim_trailing());
                                }
                                if (stack_.back() != csv_mode::subfields)
                                {
                                    before_field();
                                    if (options_.subfield_delimiter().second && curr_char == options_.subfield_delimiter().first)
                                    {
                                        before_multi_valued_field();
                                    }
                                }
                                end_quoted_string_value(ec);
                                if (ec) return;
                                if (curr_char == options_.field_delimiter())
                                {
                                    after_field();
                                }
                                state_ = csv_parse_state::before_unquoted_string;
                            }
                            ++column_;
                            ++input_ptr_;
                            break;
                    }
                    break;
                case csv_parse_state::before_unquoted_string: 
                {
                    value_buffer_.clear();
                    state_ = csv_parse_state::unquoted_string;
                    break;
                }
                case csv_parse_state::unquoted_string: 
                {
                    switch (curr_char)
                    {
                        case '\n':
                        case '\r':
                        {
                            after_newline();
                            state_ = csv_parse_state::end_record;
                            break;
                        }
                        default:
                            if (curr_char == options_.field_delimiter() || (options_.subfield_delimiter().second && curr_char == options_.subfield_delimiter().first))
                            {
                                if (options_.trim_leading() || options_.trim_trailing())
                                {
                                    trim_string_buffer(options_.trim_leading(),options_.trim_trailing());
                                }
                                if (stack_.back() != csv_mode::subfields)
                                {
                                    before_field();
                                    if (options_.subfield_delimiter().second && curr_char == options_.subfield_delimiter().first)
                                    {
                                        before_multi_valued_field();
                                    }
                                }
                                end_unquoted_string_value();
                                if (curr_char == options_.field_delimiter())
                                {
                                    after_field();
                                }
                                state_ = csv_parse_state::before_unquoted_string;
                            }
                            else if (curr_char == options_.quote_char())
                            {
                                value_buffer_.clear();
                                state_ = csv_parse_state::quoted_string;
                            }
                            else
                            {
                                value_buffer_.push_back(static_cast<CharT>(curr_char));
                            }
                            ++column_;
                            ++input_ptr_;
                            break;
                    }
                    break;
                }
                case csv_parse_state::expect_record: 
                {
                    switch (curr_char)
                    {
                        case '\n':
                        {
                            if (!options_.ignore_empty_lines())
                            {
                                before_record();
                                state_ = csv_parse_state::end_record;
                            }
                            else
                            {
                                ++line_;
                                column_ = 1;
                                state_ = csv_parse_state::expect_comment_or_record;
                                ++input_ptr_;
                            }
                            break;
                        }
                        case '\r':
                            if (!options_.ignore_empty_lines())
                            {
                                before_record();
                                state_ = csv_parse_state::end_record;
                            }
                            else
                            {
                                ++line_;
                                column_ = 1;
                                state_ = csv_parse_state::expect_comment_or_record;
                                ++input_ptr_;
                                push_state(state_);
                                state_ = csv_parse_state::cr;
                            }
                            break;
                        case ' ':
                        case '\t':
                            if (!options_.trim_leading())
                            {
                                value_buffer_.push_back(static_cast<CharT>(curr_char));
                                before_record();
                                state_ = csv_parse_state::unquoted_string;
                            }
                            ++column_;
                            ++input_ptr_;
                            break;
                        default:
                            before_record();
                            if (curr_char == options_.quote_char())
                            {
                                value_buffer_.clear();
                                state_ = csv_parse_state::quoted_string;
                                ++column_;
                                ++input_ptr_;
                            }
                            else
                            {
                                state_ = csv_parse_state::unquoted_string;
                            }
                            break;
                        }
                    break;
                    }
                case csv_parse_state::end_record: 
                {
                    switch (curr_char)
                    {
                        case '\n':
                        {
                            ++line_;
                            column_ = 1;
                            state_ = csv_parse_state::expect_comment_or_record;
                            after_record();
                            ++input_ptr_;
                            break;
                        }
                        case '\r':
                            ++line_;
                            column_ = 1;
                            state_ = csv_parse_state::expect_comment_or_record;
                            after_record();
                            push_state(state_);
                            state_ = csv_parse_state::cr;
                            ++input_ptr_;
                            break;
                        case ' ':
                        case '\t':
                            ++column_;
                            ++input_ptr_;
                            break;
                        default:
                            err_handler_(csv_errc::invalid_csv_text, *this);
                            ec = csv_errc::invalid_csv_text;
                            continue_ = false;
                            return;
                        }
                    break;
                }
                default:
                    err_handler_(csv_errc::invalid_state, *this);
                    ec = csv_errc::invalid_state;
                    continue_ = false;
                    return;
            }
            if (line_ > options_.max_lines())
            {
                state_ = csv_parse_state::done;
                continue_ = false;
            }
        }
    }

    void finish_parse()
    {
        std::error_code ec;
        finish_parse(ec);
        if (ec)
        {
            throw ser_error(ec,line_,column_);
        }
    }

    void finish_parse(std::error_code& ec)
    {
        while (continue_)
        {
            parse_some(ec);
        }
    }

    csv_parse_state state() const
    {
        return state_;
    }

    void update(const string_view_type sv)
    {
        update(sv.data(),sv.length());
    }

    void update(const CharT* data, size_t length)
    {
        begin_input_ = data;
        input_end_ = data + length;
        input_ptr_ = begin_input_;
    }

    size_t line() const override
    {
        return line_;
    }

    size_t column() const override
    {
        return column_;
    }
private:
    // name
    void before_field()
    {
        switch (stack_.back())
        {
            case csv_mode::header:
                break;
            case csv_mode::data:
            case csv_mode::subfields:
                switch (options_.mapping())
                {
                    case mapping_type::n_objects:
                        if (!(options_.ignore_empty_values() && value_buffer_.size() == 0))
                        {
                            if (column_index_ < column_names_.size() + offset_)
                            {
                                continue_ = handler_.name(column_names_[column_index_ - offset_], *this);
                            }
                        }
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }

    // begin_array
    void before_multi_valued_field()
    {
        stack_.push_back(csv_mode::subfields);
        continue_ = handler_.begin_array(semantic_tag::none, *this);
    }

    void after_field()
    {
        if (stack_.back() == csv_mode::subfields)
        {
            stack_.pop_back();
            continue_ = handler_.end_array(*this);
        }
        ++column_index_;
    }

    // begin_array or begin_record
    void before_record()
    {
        offset_ = 0;

        switch (stack_.back())
        {
            case csv_mode::header:
                if (options_.assume_header() && line_ == 1)
                {
                    if (options_.mapping() == mapping_type::n_rows)
                    {
                        continue_ = handler_.begin_array(semantic_tag::none, *this);
                    }
                }
                break;
            case csv_mode::data:
                switch (options_.mapping())
                {
                    case mapping_type::n_rows:
                        continue_ = handler_.begin_array(semantic_tag::none, *this);
                        break;
                    case mapping_type::n_objects:
                        continue_ = handler_.begin_object(semantic_tag::none, *this);
                        break;
                    case mapping_type::m_columns:
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }

    // end_array, begin_array, string_value (headers)
    void after_record()
    {
        if (column_types_.size() > 0)
        {
            if (level_ > 0)
            {
                continue_ = handler_.end_array(*this);
                level_ = 0;
            }
        }
        switch (stack_.back())
        {
            case csv_mode::header:
                if (line_ >= options_.header_lines())
                {
                    stack_.back() = csv_mode::data;
                }
                column_values_.resize(column_names_.size());
                switch (options_.mapping())
                {
                    case mapping_type::n_rows:
                        if (options_.assume_header())
                        {
                            continue_ = handler_.end_array(*this);
                        }
                        break;
                    case mapping_type::m_columns:
                        m_columns_filter_.initialize(column_names_);
                        break;
                    default:
                        break;
                }
                break;
            case csv_mode::data:
            case csv_mode::subfields:
            {
                switch (options_.mapping())
                {
                    case mapping_type::n_rows:
                        continue_ = handler_.end_array(*this);
                        break;
                    case mapping_type::n_objects:
                        continue_ = handler_.end_object(*this);
                        break;
                    case mapping_type::m_columns:
                        continue_ = handler_.end_array(*this);
                        break;
                }
                break;
            }
            default:
                break;
        }
        column_index_ = 0;
    }

    void trim_string_buffer(bool trim_leading, bool trim_trailing)
    {
        size_t start = 0;
        size_t length = value_buffer_.length();
        if (trim_leading)
        {
            bool done = false;
            while (!done && start < value_buffer_.length())
            {
                if ((value_buffer_[start] < 256) && std::isspace(value_buffer_[start]))
                {
                    ++start;
                }
                else
                {
                    done = true;
                }
            }
        }
        if (trim_trailing)
        {
            bool done = false;
            while (!done && length > 0)
            {
                if ((value_buffer_[length-1] < 256) && std::isspace(value_buffer_[length-1]))
                {
                    --length;
                }
                else
                {
                    done = true;
                }
            }
        }
        if (start != 0 || length != value_buffer_.size())
        {
            value_buffer_ = value_buffer_.substr(start,length-start);
        }
    }

    /*
        end_array, begin_array, xxx_value (end_value)
    */
    void end_unquoted_string_value() 
    {
        switch (stack_.back())
        {
            case csv_mode::header:
                if (options_.assume_header() && line_ == 1)
                {
                    column_names_.push_back(value_buffer_);
                    if (options_.mapping() == mapping_type::n_rows)
                    {
                        continue_ = handler_.string_value(value_buffer_, semantic_tag::none, *this);
                    }
                }
                break;
            case csv_mode::data:
            case csv_mode::subfields:
                switch (options_.mapping())
                {
                case mapping_type::n_rows:
                    if (options_.unquoted_empty_value_is_null() && value_buffer_.length() == 0)
                    {
                        continue_ = handler_.null_value(semantic_tag::none, *this);
                    }
                    else
                    {
                        end_value(value_buffer_,options_.infer_types());
                    }
                    break;
                case mapping_type::n_objects:
                    if (!(options_.ignore_empty_values() && value_buffer_.size() == 0))
                    {
                        if (column_index_ < column_names_.size() + offset_)
                        {
                            if (options_.unquoted_empty_value_is_null() && value_buffer_.length() == 0)
                            {
                                continue_ = handler_.null_value(semantic_tag::none, *this);
                            }
                            else
                            {
                                end_value(value_buffer_,options_.infer_types());
                            }
                        }
                        else if (level_ > 0)
                        {
                            if (options_.unquoted_empty_value_is_null() && value_buffer_.length() == 0)
                            {
                                continue_ = handler_.null_value(semantic_tag::none, *this);
                            }
                            else
                            {
                                end_value(value_buffer_,options_.infer_types());
                            }
                        }
                    }
                    break;
                case mapping_type::m_columns:
                    if (!(options_.ignore_empty_values() && value_buffer_.size() == 0))
                    {
                        end_value(value_buffer_,options_.infer_types());
                    }
                    else
                    {
                        m_columns_filter_.skip_column();
                    }
                    break;
                }
                break;
            default:
                break;
        }
    }

    void end_quoted_string_value(std::error_code& ec) 
    {
        if (options_.trim_leading_inside_quotes() | options_.trim_trailing_inside_quotes())
        {
            trim_string_buffer(options_.trim_leading_inside_quotes(),options_.trim_trailing_inside_quotes());
        }
        switch (stack_.back())
        {
            case csv_mode::header:
                if (options_.assume_header() && line_ == 1)
                {
                    column_names_.push_back(value_buffer_);
                    if (options_.mapping() == mapping_type::n_rows)
                    {
                        continue_ = handler_.string_value(value_buffer_, semantic_tag::none, *this);
                    }
                }
                break;
            case csv_mode::data:
            case csv_mode::subfields:
                switch (options_.mapping())
                {
                case mapping_type::n_rows:
                    end_value(value_buffer_,false);
                    break;
                case mapping_type::n_objects:
                    if (!(options_.ignore_empty_values() && value_buffer_.size() == 0))
                    {
                        if (column_index_ < column_names_.size() + offset_)
                        {
                            if (options_.unquoted_empty_value_is_null() && value_buffer_.length() == 0)
                            {
                                continue_ = handler_.null_value(semantic_tag::none, *this);
                            }
                            else 
                            {
                                end_value(value_buffer_,false);
                            }
                        }
                        else if (level_ > 0)
                        {
                            if (options_.unquoted_empty_value_is_null() && value_buffer_.length() == 0)
                            {
                                continue_ = handler_.null_value(semantic_tag::none, *this);
                            }
                            else
                            {
                                end_value(value_buffer_,false);
                            }
                        }
                    }
                    break;
                case mapping_type::m_columns:
                    if (!(options_.ignore_empty_values() && value_buffer_.size() == 0))
                    {
                        end_value(value_buffer_,options_.infer_types());
                    }
                    else
                    {
                        m_columns_filter_.skip_column();
                    }
                    break;
                }
                break;
            default:
                err_handler_(csv_errc::invalid_csv_text, *this);
                ec = csv_errc::invalid_csv_text;
                continue_ = false;
                return;
        }
    }

    void end_value(const string_view_type& value, bool infer_types)
    {
        if (column_index_ < column_types_.size() + offset_)
        {
            if (column_types_[column_index_ - offset_].col_type == csv_column_type::repeat_t)
            {
                offset_ = offset_ + column_types_[column_index_ - offset_].rep_count;
                if (column_index_ - offset_ + 1 < column_types_.size())
                {
                    if (column_index_ == offset_ || level_ > column_types_[column_index_-offset_].level)
                    {
                        continue_ = handler_.end_array(*this);
                    }
                    level_ = column_index_ == offset_ ? 0 : column_types_[column_index_ - offset_].level;
                }
            }
            if (level_ < column_types_[column_index_ - offset_].level)
            {
                continue_ = handler_.begin_array(semantic_tag::none, *this);
                level_ = column_types_[column_index_ - offset_].level;
            }
            else if (level_ > column_types_[column_index_ - offset_].level)
            {
                continue_ = handler_.end_array(*this);
                level_ = column_types_[column_index_ - offset_].level;
            }
            switch (column_types_[column_index_ - offset_].col_type)
            {
                case csv_column_type::integer_t:
                    {
                        std::istringstream iss{ std::string(value) };
                        int64_t val;
                        iss >> val;
                        if (!iss.fail())
                        {
                            continue_ = handler_.int64_value(val, semantic_tag::none, *this);
                        }
                        else
                        {
                            if (column_index_ - offset_ < column_defaults_.size() && column_defaults_[column_index_ - offset_].length() > 0)
                            {
                                basic_json_parser<CharT> parser;
                                parser.update(column_defaults_[column_index_ - offset_].data(),column_defaults_[column_index_ - offset_].length());
                                parser.parse_some(handler_);
                                parser.finish_parse(handler_);
                            }
                            else
                            {
                                continue_ = handler_.null_value(semantic_tag::none, *this);
                            }
                        }
                    }
                    break;
                case csv_column_type::float_t:
                    {
                        if (options_.lossless_number())
                        {
                            continue_ = handler_.string_value(value,semantic_tag::bigdec, *this);
                        }
                        else
                        {
                            std::istringstream iss{ std::string(value) };
                            double val;
                            iss >> val;
                            if (!iss.fail())
                            {
                                continue_ = handler_.double_value(val, semantic_tag::none, *this);
                            }
                            else
                            {
                                if (column_index_ - offset_ < column_defaults_.size() && column_defaults_[column_index_ - offset_].length() > 0)
                                {
                                    basic_json_parser<CharT> parser;
                                    parser.update(column_defaults_[column_index_ - offset_].data(),column_defaults_[column_index_ - offset_].length());
                                    parser.parse_some(handler_);
                                    parser.finish_parse(handler_);
                                }
                                else
                                {
                                    continue_ = handler_.null_value(semantic_tag::none, *this);
                                }
                            }
                        }
                    }
                    break;
                case csv_column_type::boolean_t:
                    {
                        if (value.length() == 1 && value[0] == '0')
                        {
                            continue_ = handler_.bool_value(false, semantic_tag::none, *this);
                        }
                        else if (value.length() == 1 && value[0] == '1')
                        {
                            continue_ = handler_.bool_value(true, semantic_tag::none, *this);
                        }
                        else if (value.length() == 5 && ((value[0] == 'f' || value[0] == 'F') && (value[1] == 'a' || value[1] == 'A') && (value[2] == 'l' || value[2] == 'L') && (value[3] == 's' || value[3] == 'S') && (value[4] == 'e' || value[4] == 'E')))
                        {
                            continue_ = handler_.bool_value(false, semantic_tag::none, *this);
                        }
                        else if (value.length() == 4 && ((value[0] == 't' || value[0] == 'T') && (value[1] == 'r' || value[1] == 'R') && (value[2] == 'u' || value[2] == 'U') && (value[3] == 'e' || value[3] == 'E')))
                        {
                            continue_ = handler_.bool_value(true, semantic_tag::none, *this);
                        }
                        else
                        {
                            if (column_index_ - offset_ < column_defaults_.size() && column_defaults_[column_index_ - offset_].length() > 0)
                            {
                                basic_json_parser<CharT> parser;
                                parser.update(column_defaults_[column_index_ - offset_].data(),column_defaults_[column_index_ - offset_].length());
                                parser.parse_some(handler_);
                                parser.finish_parse(handler_);
                            }
                            else
                            {
                                continue_ = handler_.null_value(semantic_tag::none, *this);
                            }
                        }
                    }
                    break;
                default:
                    if (value.length() > 0)
                    {
                        continue_ = handler_.string_value(value, semantic_tag::none, *this);
                    }
                    else
                    {
                        if (column_index_ < column_defaults_.size() + offset_ && column_defaults_[column_index_ - offset_].length() > 0)
                        {
                            basic_json_parser<CharT> parser;
                            parser.update(column_defaults_[column_index_ - offset_].data(),column_defaults_[column_index_ - offset_].length());
                            parser.parse_some(handler_);
                            parser.finish_parse(handler_);
                        }
                        else
                        {
                            continue_ = handler_.string_value(string_view_type(), semantic_tag::none, *this);
                        }
                    }
                    break;  
            }
        }
        else
        {
            if (infer_types)
            {
                end_value_with_numeric_check(value);
            }
            else
            {
                continue_ = handler_.string_value(value, semantic_tag::none, *this);
            }
        }
    }

    enum class numeric_check_state 
    {
        initial,
        null,
        boolean_true,
        boolean_false,
        minus,
        zero,
        integer,
        fraction1,
        fraction,
        exp1,
        exp,
        done
    };

    /*
        xxx_value 
    */
    void end_value_with_numeric_check(const string_view_type& value)
    {
        numeric_check_state state = numeric_check_state::initial;
        bool is_negative = false;
        int precision = 0;
        uint8_t decimal_places = 0;

        auto last = value.end();

        std::string buffer;
        for (auto p = value.begin(); state != numeric_check_state::done && p != last; ++p)
        {
            switch (state)
            {
                case numeric_check_state::initial:
                {
                    switch (*p)
                    {
                    case 'n':case 'N':
                        if ((last-p) == 4 && (p[1] == 'u' || p[1] == 'U') && (p[2] == 'l' || p[2] == 'L') && (p[3] == 'l' || p[3] == 'L'))
                        {
                            state = numeric_check_state::null;
                        }
                        else
                        {
                            state = numeric_check_state::done;
                        }
                        break;
                    case 't':case 'T':
                        if ((last-p) == 4 && (p[1] == 'r' || p[1] == 'R') && (p[2] == 'u' || p[2] == 'U') && (p[3] == 'e' || p[3] == 'U'))
                        {
                            state = numeric_check_state::boolean_true;
                        }
                        else
                        {
                            state = numeric_check_state::done;
                        }
                        break;
                    case 'f':case 'F':
                        if ((last-p) == 5 && (p[1] == 'a' || p[1] == 'A') && (p[2] == 'l' || p[2] == 'L') && (p[3] == 's' || p[3] == 'S') && (p[4] == 'e' || p[4] == 'E'))
                        {
                            state = numeric_check_state::boolean_false;
                        }
                        else
                        {
                            state = numeric_check_state::done;
                        }
                        break;
                    case '-':
                        is_negative = true;
                        buffer.push_back(*p);
                        state = numeric_check_state::minus;
                        break;
                    case '0':
                        ++precision;
                        buffer.push_back(*p);
                        state = numeric_check_state::zero;
                        break;
                    case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8':case '9':
                        ++precision;
                        buffer.push_back(*p);
                        state = numeric_check_state::integer;
                        break;
                    default:
                        state = numeric_check_state::done;
                        break;
                    }
                    break;
                }
                case numeric_check_state::zero:
                {
                    switch (*p)
                    {
                    case '.':
                        buffer.push_back(to_double_.get_decimal_point());
                        state = numeric_check_state::fraction1;
                        break;
                    case 'e':case 'E':
                        buffer.push_back(*p);
                        state = numeric_check_state::exp1;
                        break;
                    default:
                        state = numeric_check_state::done;
                        break;
                    }
                    break;
                }
                case numeric_check_state::integer:
                {
                    switch (*p)
                    {
                    case '0':case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8':case '9':
                        ++precision;
                        buffer.push_back(*p);
                        break;
                    case '.':
                        buffer.push_back(to_double_.get_decimal_point());
                        state = numeric_check_state::fraction1;
                        break;
                    case 'e':case 'E':
                        buffer.push_back(*p);
                        state = numeric_check_state::exp1;
                        break;
                    default:
                        state = numeric_check_state::done;
                        break;
                    }
                    break;
                }
                case numeric_check_state::minus:
                {
                    switch (*p)
                    {
                    case '0':
                        ++precision;
                        buffer.push_back(*p);
                        state = numeric_check_state::zero;
                        break;
                    case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8':case '9':
                        ++precision;
                        buffer.push_back(*p);
                        state = numeric_check_state::integer;
                        break;
                    case 'e':case 'E':
                        buffer.push_back(*p);
                        state = numeric_check_state::exp1;
                        break;
                    default:
                        state = numeric_check_state::done;
                        break;
                    }
                    break;
                }
                case numeric_check_state::fraction1:
                {
                    switch (*p)
                    {
                    case '0':case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8':case '9':
                        ++precision;
                        ++decimal_places;
                        buffer.push_back(*p);
                        state = numeric_check_state::fraction;
                        break;
                    default:
                        state = numeric_check_state::done;
                        break;
                    }
                    break;
                }
                case numeric_check_state::fraction:
                {
                    switch (*p)
                    {
                    case '0':case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8':case '9':
                        ++precision;
                        ++decimal_places;
                        buffer.push_back(*p);
                        break;
                    case 'e':case 'E':
                        buffer.push_back(*p);
                        state = numeric_check_state::exp1;
                        break;
                    default:
                        state = numeric_check_state::done;
                        break;
                    }
                    break;
                }
                case numeric_check_state::exp1:
                {
                    switch (*p)
                    {
                    case '-':
                        buffer.push_back(*p);
                        state = numeric_check_state::exp;
                        break;
                    case '+':
                        state = numeric_check_state::exp;
                        break;
                    case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8':case '9':
                        buffer.push_back(*p);
                        state = numeric_check_state::integer;
                        break;
                    default:
                        state = numeric_check_state::done;
                        break;
                    }
                    break;
                }
                case numeric_check_state::exp:
                {
                    switch (*p)
                    {
                    case '0':case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8':case '9':
                        buffer.push_back(*p);
                        break;
                    default:
                        state = numeric_check_state::done;
                        break;
                    }
                    break;
                }
                default:
                    break;
            }
        }

        switch (state)
        {
            case numeric_check_state::null:
                continue_ = handler_.null_value(semantic_tag::none, *this);
                break;
            case numeric_check_state::boolean_true:
                continue_ = handler_.bool_value(true, semantic_tag::none, *this);
                break;
            case numeric_check_state::boolean_false:
                continue_ = handler_.bool_value(false, semantic_tag::none, *this);
                break;
            case numeric_check_state::zero:
            case numeric_check_state::integer:
            {
                if (is_negative)
                {
                    auto result = jsoncons::detail::to_integer<int64_t>(value.data(), value.length());
                    if (result.ec == jsoncons::detail::to_integer_errc())
                    {
                        continue_ = handler_.int64_value(result.value, semantic_tag::none, *this);
                    }
                    else // Must be overflow
                    {
                        continue_ = handler_.string_value(value, semantic_tag::bigint, *this);
                    }
                }
                else
                {
                    auto result = jsoncons::detail::to_integer<uint64_t>(value.data(), value.length());
                    if (result.ec == jsoncons::detail::to_integer_errc())
                    {
                        continue_ = handler_.uint64_value(result.value, semantic_tag::none, *this);
                    }
                    else if (result.ec == jsoncons::detail::to_integer_errc::overflow)
                    {
                        continue_ = handler_.string_value(value, semantic_tag::bigint, *this);
                    }
                    else
                    {
                        JSONCONS_THROW(json_runtime_error<std::invalid_argument>(make_error_code(result.ec).message()));
                    }
                }
                break;
            }
            case numeric_check_state::fraction:
            case numeric_check_state::exp:
            {
                if (options_.lossless_number())
                {
                    continue_ = handler_.string_value(value,semantic_tag::bigdec, *this);
                }
                else
                {
                    double d = to_double_(buffer.c_str(), buffer.length());
                    continue_ = handler_.double_value(d, semantic_tag::none, *this);
                }
                break;
            }
            default:
            {
                continue_ = handler_.string_value(value, semantic_tag::none, *this);
                break;
            }
        }
    } 

    void push_state(csv_parse_state state)
    {
        state_stack_.push_back(state);
    }

    csv_parse_state pop_state()
    {
        JSONCONS_ASSERT(!state_stack_.empty())
        csv_parse_state state = state_stack_.back();
        state_stack_.pop_back();
        return state;
    }

    void after_newline()
    {
        if (options_.trim_leading() || options_.trim_trailing())
        {
            trim_string_buffer(options_.trim_leading(),options_.trim_trailing());
        }
        if (!options_.ignore_empty_lines() || (column_index_ > 0 || value_buffer_.length() > 0))
        {
            if (stack_.back() != csv_mode::subfields)
            {
                before_field();
            }
            end_unquoted_string_value();
            after_field();
        }
    }

    void after_newline_between_fields(std::error_code& ec)
    {
        if (options_.trim_leading() || options_.trim_trailing())
        {
            trim_string_buffer(options_.trim_leading(),options_.trim_trailing());
        }
        if (!options_.ignore_empty_lines() || (column_index_ > 0 || value_buffer_.length() > 0))
        {
            if (stack_.back() != csv_mode::subfields)
            {
                before_field();
            }
            end_quoted_string_value(ec);
            if (ec) return;
            after_field();
        }
    }
};

typedef basic_csv_parser<char> csv_parser;
typedef basic_csv_parser<wchar_t> wcsv_parser;

}}

#endif

