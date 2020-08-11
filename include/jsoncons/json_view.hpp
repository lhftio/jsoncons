// Copyright 2013 Daniel Parker
// Distributed under the Boost license, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// See https://github.com/danielaparker/jsoncons for latest version

#ifndef JSONCONS_JSON_VIEW_HPP
#define JSONCONS_JSON_VIEW_HPP

#include <limits> // std::numeric_limits
#include <string>
#include <vector>
#include <exception>
#include <cstring>
#include <ostream>
#include <memory> // std::allocator
#include <typeinfo>
#include <cstring> // std::memcpy
#include <algorithm> // std::swap
#include <initializer_list> // std::initializer_list
#include <utility> // std::move
#include <type_traits> // std::enable_if
#include <istream> // std::basic_istream
#include <jsoncons/json_fwd.hpp>
#include <jsoncons/json_type.hpp>
#include <jsoncons/config/version.hpp>
#include <jsoncons/json_type.hpp>
#include <jsoncons/json_exception.hpp>
#include <jsoncons/pretty_print.hpp>
#include <jsoncons/json_container_types.hpp>
#include <jsoncons/bigint.hpp>
#include <jsoncons/json_options.hpp>
#include <jsoncons/json_encoder.hpp>
#include <jsoncons/json_decoder.hpp>
#include <jsoncons/json_reader.hpp>
#include <jsoncons/json_type_traits.hpp>
#include <jsoncons/byte_string.hpp>
#include <jsoncons/json_error.hpp>
#include <jsoncons/detail/string_wrapper.hpp>
#include <jsoncons/json.hpp>

namespace jsoncons {

enum class view_storage_kind : uint8_t 
{
    null_value = 0x00,
    array_value = 0x09,
    empty_object_value = 0x0a,
    object_value = 0x0b,
    json_const_pointer = 0x0c
};

struct json_const_pointer_arg_t
{
    explicit json_const_pointer_arg_t() = default; 
};

constexpr json_const_pointer_arg_t json_const_pointer_arg{};

template <class Json>
class json_view
{
public:

    using allocator_type = typename Json::allocator_type; 

    using implementation_policy = typename Json::implementation_policy;

    using parse_error_handler_type = typename implementation_policy::parse_error_handler_type;

    using char_type = typename Json::char_type;
    using char_traits_type = std::char_traits<char_type>;
    using string_view_type = jsoncons::basic_string_view<char_type,char_traits_type>;

    using char_allocator_type = typename std::allocator_traits<allocator_type>:: template rebind_alloc<char_type>;

    using key_type = std::basic_string<char_type,char_traits_type,char_allocator_type>;


    using reference = json_view&;
    using const_reference = const json_view&;
    using pointer = json_view*;
    using const_pointer = const json_view*;

    using key_value_type = key_value<key_type,json_view>;

    using array = json_array<json_view>;

    using key_value_allocator_type = typename std::allocator_traits<allocator_type>:: template rebind_alloc<key_value_type>;                       

    using object = json_object<key_type,json_view>;

    using object_iterator = jsoncons::detail::random_access_iterator_wrapper<typename object::iterator>;              
    using const_object_iterator = jsoncons::detail::random_access_iterator_wrapper<typename object::const_iterator>;                    
    using array_iterator = typename array::iterator;
    using const_array_iterator = typename array::const_iterator;

private:

    static constexpr uint8_t major_type_shift = 0x04;
    static constexpr uint8_t additional_information_mask = (1U << 4) - 1;

    class common_storage final
    {
    public:
        uint8_t storage_:4;
        uint8_t length_:4;
        semantic_tag tag_;
    };

    class null_storage final
    {
    public:
        uint8_t storage_:4;
        uint8_t length_:4;
        semantic_tag tag_;

        null_storage(semantic_tag tag = semantic_tag::none)
            : storage_(static_cast<uint8_t>(view_storage_kind::null_value)), length_(0), tag_(tag)
        {
        }
    };

    class empty_object_storage final
    {
    public:
        uint8_t storage_:4;
        uint8_t length_:4;
        semantic_tag tag_;

        empty_object_storage(semantic_tag tag)
            : storage_(static_cast<uint8_t>(view_storage_kind::empty_object_value)), length_(0), tag_(tag)
        {
        }
    };  

    // array_storage
    class array_storage final
    {
    public:
        uint8_t storage_:4;
        uint8_t length_:4;
        semantic_tag tag_;
    private:
        using array_allocator = typename std::allocator_traits<allocator_type>:: template rebind_alloc<array>;
        using pointer = typename std::allocator_traits<array_allocator>::pointer;

        pointer ptr_;

        template <typename... Args>
        void create(array_allocator alloc, Args&& ... args)
        {
            ptr_ = std::allocator_traits<array_allocator>::allocate(alloc, 1);
            JSONCONS_TRY
            {
                std::allocator_traits<array_allocator>::construct(alloc, jsoncons::detail::to_plain_pointer(ptr_), std::forward<Args>(args)...);
            }
            JSONCONS_CATCH(...)
            {
                std::allocator_traits<array_allocator>::deallocate(alloc, ptr_,1);
                JSONCONS_RETHROW;
            }
        }

        void destroy() noexcept
        {
            array_allocator alloc(ptr_->get_allocator());
            std::allocator_traits<array_allocator>::destroy(alloc, jsoncons::detail::to_plain_pointer(ptr_));
            std::allocator_traits<array_allocator>::deallocate(alloc, ptr_,1);
        }
    public:
        array_storage(const array& val, semantic_tag tag)
            : storage_(static_cast<uint8_t>(view_storage_kind::array_value)), length_(0), tag_(tag)
        {
            create(val.get_allocator(), val);
        }

        array_storage(const array& val, semantic_tag tag, const allocator_type& a)
            : storage_(val.storage_), length_(0), tag_(val.tag_)
        {
            create(array_allocator(a), val, a);
        }

        array_storage(const array_storage& val)
            : storage_(val.storage_), length_(0), tag_(val.tag_)
        {
            create(val.ptr_->get_allocator(), *(val.ptr_));
        }

        array_storage(array_storage&& val) noexcept
            : storage_(val.storage_), length_(0), tag_(val.tag_),
              ptr_(nullptr)
        {
            std::swap(val.ptr_, ptr_);
        }

        array_storage(const array_storage& val, const allocator_type& a)
            : storage_(val.storage_), length_(0), tag_(val.tag_)
        {
            create(array_allocator(a), *(val.ptr_), a);
        }
        ~array_storage() noexcept
        {
            if (ptr_ != nullptr)
            {
                destroy();
            }
        }

        allocator_type get_allocator() const
        {
            return ptr_->get_allocator();
        }

        void swap(array_storage& val) noexcept
        {
            std::swap(val.ptr_,ptr_);
        }

        array& value()
        {
            return *ptr_;
        }

        const array& value() const
        {
            return *ptr_;
        }
    };

    // object_storage
    class object_storage final
    {
    public:
        uint8_t storage_:4;
        uint8_t length_:4;
        semantic_tag tag_;
    private:
        using object_allocator = typename std::allocator_traits<allocator_type>:: template rebind_alloc<object>;
        using pointer = typename std::allocator_traits<object_allocator>::pointer;

        pointer ptr_;

        template <typename... Args>
        void create(object_allocator alloc, Args&& ... args)
        {
            ptr_ = std::allocator_traits<object_allocator>::allocate(alloc, 1);
            JSONCONS_TRY
            {
                std::allocator_traits<object_allocator>::construct(alloc, jsoncons::detail::to_plain_pointer(ptr_), std::forward<Args>(args)...);
            }
            JSONCONS_CATCH(...)
            {
                std::allocator_traits<object_allocator>::deallocate(alloc, ptr_,1);
                JSONCONS_RETHROW;
            }
        }
    public:
        explicit object_storage(const object& val, semantic_tag tag)
            : storage_(static_cast<uint8_t>(view_storage_kind::object_value)), length_(0), tag_(tag)
        {
            create(val.get_allocator(), val);
        }

        explicit object_storage(const object& val, semantic_tag tag, const allocator_type& a)
            : storage_(val.storage_), length_(0), tag_(val.tag_)
        {
            create(object_allocator(a), val, a);
        }

        explicit object_storage(const object_storage& val)
            : storage_(val.storage_), length_(0), tag_(val.tag_)
        {
            create(val.ptr_->get_allocator(), *(val.ptr_));
        }

        explicit object_storage(object_storage&& val) noexcept
            : storage_(val.storage_), length_(0), tag_(val.tag_),
              ptr_(nullptr)
        {
            std::swap(val.ptr_,ptr_);
        }

        explicit object_storage(const object_storage& val, const allocator_type& a)
            : storage_(val.storage_), tag_(val.tag_)
        {
            create(object_allocator(a), *(val.ptr_), a);
        }

        ~object_storage() noexcept
        {
            if (ptr_ != nullptr)
            {
                destroy();
            }
        }

        void swap(object_storage& val) noexcept
        {
            std::swap(val.ptr_,ptr_);
        }

        object& value()
        {
            return *ptr_;
        }

        const object& value() const
        {
            return *ptr_;
        }

        allocator_type get_allocator() const
        {
            return ptr_->get_allocator();
        }
    private:

        void destroy() noexcept
        {
            object_allocator alloc(ptr_->get_allocator());
            std::allocator_traits<object_allocator>::destroy(alloc, jsoncons::detail::to_plain_pointer(ptr_));
            std::allocator_traits<object_allocator>::deallocate(alloc, ptr_,1);
        }
    };

    class json_const_pointer_storage final
    {
    public:
        uint8_t storage_:4;
        uint8_t length_:4;
        semantic_tag tag_;
    private:
        const Json* p_;
    public:
        json_const_pointer_storage(const Json* p)
            : storage_(static_cast<uint8_t>(view_storage_kind::json_const_pointer)), length_(0), tag_(p->tag()),
              p_(p)
        {
        }

        const Json* value() const
        {
            return p_;
        }
    };

    union 
    {
        common_storage common_stor_;
        null_storage null_stor_;
        array_storage array_stor_;
        object_storage object_stor_;
        empty_object_storage empty_object_stor_;
        json_const_pointer_storage json_const_pointer_stor_;
    };

    void Destroy_()
    {
        switch (storage())
        {
            case view_storage_kind::array_value:
                destroy_var<array_storage>();
                break;
            case view_storage_kind::object_value:
                destroy_var<object_storage>();
                break;
            default:
                break;
        }
    }

    template <class VariantType, class... Args>
    void construct(Args&&... args)
    {
        ::new (&cast<VariantType>()) VariantType(std::forward<Args>(args)...);
    }

    template <class T>
    void destroy_var()
    {
        cast<T>().~T();
    }

    template <class T>
    struct identity { using type = T*; };

    template <class T> 
    T& cast()
    {
        return cast(identity<T>());
    }

    template <class T> 
    const T& cast() const
    {
        return cast(identity<T>());
    }

    null_storage& cast(identity<null_storage>) 
    {
        return null_stor_;
    }

    const null_storage& cast(identity<null_storage>) const
    {
        return null_stor_;
    }

    empty_object_storage& cast(identity<empty_object_storage>) 
    {
        return empty_object_stor_;
    }

    const empty_object_storage& cast(identity<empty_object_storage>) const
    {
        return empty_object_stor_;
    }

    object_storage& cast(identity<object_storage>)
    {
        return object_stor_;
    }

    const object_storage& cast(identity<object_storage>) const
    {
        return object_stor_;
    }

    array_storage& cast(identity<array_storage>)
    {
        return array_stor_;
    }

    const array_storage& cast(identity<array_storage>) const
    {
        return array_stor_;
    }

    json_const_pointer_storage& cast(identity<json_const_pointer_storage>) 
    {
        return json_const_pointer_stor_;
    }

    const json_const_pointer_storage& cast(identity<json_const_pointer_storage>) const
    {
        return json_const_pointer_stor_;
    }

    template <class TypeA, class TypeB>
    void swap_a_b(json_view& other)
    {
        TypeA& curA = cast<TypeA>();
        TypeB& curB = other.cast<TypeB>();
        TypeB tmpB(std::move(curB));
        other.construct<TypeA>(std::move(curA));
        construct<TypeB>(std::move(tmpB));
    }

    template <class TypeA>
    void swap_a(json_view& other)
    {
        switch (other.storage())
        {
            case view_storage_kind::null_value         : swap_a_b<TypeA, null_storage>(other); break;
            case view_storage_kind::empty_object_value : swap_a_b<TypeA, empty_object_storage>(other); break;
            case view_storage_kind::array_value        : swap_a_b<TypeA, array_storage>(other); break;
            case view_storage_kind::object_value       : swap_a_b<TypeA, object_storage>(other); break;
            case view_storage_kind::json_const_pointer : swap_a_b<TypeA, json_const_pointer_storage>(other); break;
            default:
                JSONCONS_UNREACHABLE();
                break;
        }
    }

    void Init_(const json_view& val)
    {
        switch (val.storage())
        {
            case view_storage_kind::null_value:
                construct<null_storage>(val.cast<null_storage>());
                break;
            case view_storage_kind::empty_object_value:
                construct<empty_object_storage>(val.cast<empty_object_storage>());
                break;
            case view_storage_kind::object_value:
                construct<object_storage>(val.cast<object_storage>());
                break;
            case view_storage_kind::array_value:
                construct<array_storage>(val.cast<array_storage>());
                break;
            case view_storage_kind::json_const_pointer:
                Init_(*(val.cast<json_const_pointer_storage>().value()));
                break;
            default:
                break;
        }
    }

    void Init_(const json_view& val, const allocator_type& a)
    {
        switch (val.storage())
        {
            case view_storage_kind::null_value:
            case view_storage_kind::empty_object_value:
            case view_storage_kind::array_value:
                construct<array_storage>(val.cast<array_storage>(),a);
                break;
            case view_storage_kind::object_value:
                construct<object_storage>(val.cast<object_storage>(),a);
                break;
            case view_storage_kind::json_const_pointer:
                Init_(*(val.cast<json_const_pointer_storage>().value()), a);
                break;
            default:
                break;
        }
    }

    void Init_rv_(json_view&& val) noexcept
    {
        switch (val.storage())
        {
            case view_storage_kind::null_value:
            case view_storage_kind::empty_object_value:
            case view_storage_kind::json_const_pointer:
                Init_(val);
                break;
            case view_storage_kind::array_value:
            case view_storage_kind::object_value:
            {
                construct<null_storage>();
                swap(val);
                break;
            }
            default:
                JSONCONS_UNREACHABLE();
                break;
        }
    }

    void Init_rv_(json_view&& val, const allocator_type&, std::true_type) noexcept
    {
        Init_rv_(std::forward<json_view>(val));
    }

    void Init_rv_(json_view&& val, const allocator_type& a, std::false_type) noexcept
    {
        switch (val.storage())
        {
            case view_storage_kind::null_value:
            case view_storage_kind::empty_object_value:
            case view_storage_kind::json_const_pointer:
                Init_(std::forward<json_view>(val));
                break;
            case view_storage_kind::object_value:
            {
                if (a == val.cast<object_storage>().get_allocator())
                {
                    Init_rv_(std::forward<json_view>(val), a, std::true_type());
                }
                else
                {
                    Init_(val,a);
                }
                break;
            }
            case view_storage_kind::array_value:
            {
                if (a == val.cast<array_storage>().get_allocator())
                {
                    Init_rv_(std::forward<json_view>(val), a, std::true_type());
                }
                else
                {
                    Init_(val,a);
                }
                break;
            }
        default:
            break;
        }
    }

public:

    view_storage_kind storage() const
    {
        // It is legal to access 'common_stor_.storage_' even though 
        // common_stor_ is not the active member of the union because 'storage_' 
        // is a part of the common initial sequence of all union members
        // as defined in 11.4-25 of the Standard.
        return static_cast<view_storage_kind>(common_stor_.storage_);
    }

    json_type type() const
    {
        switch(storage())
        {
            case view_storage_kind::null_value:
                return json_type::null_value;
                return json_type::array_value;
            case view_storage_kind::empty_object_value:
            case view_storage_kind::object_value:
                return json_type::object_value;
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->type();
            default:
                JSONCONS_UNREACHABLE();
                break;
        }
    }

    semantic_tag tag() const
    {
        // It is legal to access 'common_stor_.tag_' even though 
        // common_stor_ is not the active member of the union because 'tag_' 
        // is a part of the common initial sequence of all union members
        // as defined in 11.4-25 of the Standard.
        switch(storage())
        {
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->tag();
            default:
                return common_stor_.tag_;
        }
    }

    std::size_t size() const
    {
        switch (storage())
        {
            case view_storage_kind::array_value:
                return cast<array_storage>().value().size();
            case view_storage_kind::empty_object_value:
                return 0;
            case view_storage_kind::object_value:
                return cast<object_storage>().value().size();
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->size();
            default:
                return 0;
        }
    }

    string_view_type as_string_view() const
    {
        switch (storage())
        {
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->as_string_view();
            default:
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Not a string"));
        }
    }

    template <typename BAllocator=std::allocator<uint8_t>>
    basic_byte_string<BAllocator> as_byte_string() const
    {
        using byte_string_type = basic_byte_string<BAllocator>;
        converter<byte_string_type> convert;
        std::error_code ec;

        switch (storage())
        {
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->as_byte_string();
            default:
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Not a byte string"));
        }
    }

    byte_string_view as_byte_string_view() const
    {
        switch (storage())
        {
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->as_byte_string_view();
            default:
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Not a byte string"));
        }
    }

    friend bool operator==(const json_view& lhs, const json_view& rhs) noexcept
    {
        if (&lhs ==&rhs)
        {
            return true;
        }
        switch (lhs.storage())
        {
            case view_storage_kind::json_const_pointer:
                switch (rhs.storage())
                {
                    case view_storage_kind::json_const_pointer:
                        return *(lhs.cast<json_const_pointer_storage>().value()) == *(rhs.cast<json_const_pointer_storage>().value());
                    default:
                        return *(lhs.cast<json_const_pointer_storage>().value()) == rhs;
                }
                break;
            case view_storage_kind::null_value:
                switch (rhs.storage())
                {
                    case view_storage_kind::null_value:
                        return true;
                    case view_storage_kind::json_const_pointer:
                        return lhs == *(rhs.cast<json_const_pointer_storage>().value());
                    default:
                        return false;
                }
                break;
            case view_storage_kind::empty_object_value:
                switch (rhs.storage())
                {
                    case view_storage_kind::empty_object_value:
                        return true;
                    case view_storage_kind::object_value:
                        return rhs.size() == 0;
                    case view_storage_kind::json_const_pointer:
                        return lhs == *(rhs.cast<json_const_pointer_storage>().value());
                    default:
                        return false;
                }
                break;
            case view_storage_kind::array_value:
                switch (rhs.storage())
                {
                    case view_storage_kind::array_value:
                        return lhs.cast<array_storage>().value() == rhs.cast<array_storage>().value();
                    case view_storage_kind::json_const_pointer:
                        return lhs == *(rhs.cast<json_const_pointer_storage>().value());
                    default:
                        return false;
                }
                break;
            case view_storage_kind::object_value:
                switch (rhs.storage())
                {
                    case view_storage_kind::empty_object_value:
                        return lhs.size() == 0;
                    case view_storage_kind::object_value:
                        return lhs.cast<object_storage>().value() == rhs.cast<object_storage>().value();
                    case view_storage_kind::json_const_pointer:
                        return lhs == *(rhs.cast<json_const_pointer_storage>().value());
                    default:
                        return false;
                }
                break;
            default:
                JSONCONS_UNREACHABLE();
                break;
        }
    }

    friend bool operator<(const json_view& lhs, const json_view& rhs) noexcept
    {
        if (&lhs == &rhs)
        {
            return false;
        }
        switch (lhs.storage())
        {
            case view_storage_kind::json_const_pointer:
                switch (rhs.storage())
                {
                    case view_storage_kind::json_const_pointer:
                        return *(lhs.cast<json_const_pointer_storage>().value()) < *(rhs.cast<json_const_pointer_storage>().value());
                    default:
                        return *(lhs.cast<json_const_pointer_storage>().value()) < rhs;
                }
                break;
            case view_storage_kind::null_value:
                return (int)lhs.storage() < (int)rhs.storage();
            case view_storage_kind::empty_object_value:
                switch (rhs.storage())
                {
                    case view_storage_kind::empty_object_value:
                        return false;
                    case view_storage_kind::object_value:
                        return rhs.size() != 0;
                    case view_storage_kind::json_const_pointer:
                        return lhs < *(rhs.cast<json_const_pointer_storage>().value());
                    default:
                        return (int)lhs.storage() < (int)rhs.storage();
                }
                break;
            case view_storage_kind::array_value:
                switch (rhs.storage())
                {
                    case view_storage_kind::array_value:
                        return lhs.cast<array_storage>().value() < rhs.cast<array_storage>().value();
                    case view_storage_kind::json_const_pointer:
                        return lhs < *(rhs.cast<json_const_pointer_storage>().value());
                    default:
                        return (int)lhs.storage() < (int)rhs.storage();
                }
                break;
            case view_storage_kind::object_value:
                switch (rhs.storage())
                {
                    case view_storage_kind::empty_object_value:
                        return false;
                    case view_storage_kind::object_value:
                        return lhs.cast<object_storage>().value() < rhs.cast<object_storage>().value();
                    case view_storage_kind::json_const_pointer:
                        return lhs < *(rhs.cast<json_const_pointer_storage>().value());
                    default:
                        return (int)lhs.storage() < (int)rhs.storage();
                }
                break;
            default:
                JSONCONS_UNREACHABLE();
                break;
        }
    }

    friend bool operator!=(const json_view& lhs, const json_view& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    friend bool operator<=(const json_view& lhs, const json_view& rhs) noexcept
    {
        return !(rhs < lhs);
    }

    friend bool operator>(const json_view& lhs, const json_view& rhs) noexcept
    {
        return !(lhs <= rhs);
    }

    friend bool operator>=(const json_view& lhs, const json_view& rhs) noexcept
    {
        return !(lhs < rhs);
    }

    void swap(json_view& other) noexcept
    {
        if (this == &other)
        {
            return;
        }

        switch (storage())
        {
            case view_storage_kind::null_value: swap_a<null_storage>(other); break;
            case view_storage_kind::empty_object_value : swap_a<empty_object_storage>(other); break;
            case view_storage_kind::array_value: swap_a<array_storage>(other); break;
            case view_storage_kind::object_value: swap_a<object_storage>(other); break;
            case view_storage_kind::json_const_pointer: swap_a<json_const_pointer_storage>(other); break;
            default:
                JSONCONS_UNREACHABLE();
                break;
        }
    }

    static const json_view& null()
    {
        static const Json a_null(null_type(), semantic_tag::none);
        return a_null;
    }

    json_view() 
    {
        construct<empty_object_storage>(semantic_tag::none);
    }

    json_view(const json_view& other)
    {
        Init_(other);
    }

    json_view(const json_view& other, const allocator_type& alloc)
    {
        Init_(other,alloc);
    }

    json_view(json_view&& other) noexcept
    {
        Init_rv_(std::forward<json_view>(other));
    }

    json_view(json_view&& other, const allocator_type&) noexcept
    {
        Init_rv_(std::forward<json_view>(other));
    }

    explicit json_view(json_object_arg_t, 
                        semantic_tag tag = semantic_tag::none,
                        const allocator_type& alloc = allocator_type()) 
    {
        construct<object_storage>(object(alloc), tag);
    }

    explicit json_view(json_array_arg_t, 
                        semantic_tag tag = semantic_tag::none, 
                        const allocator_type& alloc = allocator_type()) 
    {
        construct<array_storage>(array(alloc), tag);
    }

    json_view(json_const_pointer_arg_t, const Json* p) noexcept 
    {
        if (p == nullptr)
        {
            construct<null_storage>(semantic_tag::none);
        }
        else
        {
            construct<json_const_pointer_storage>(p);
        }
    }

    ~json_view() noexcept
    {
         Destroy_();
    }

    json_view& operator=(const json_view& val)
    {
        if (this != &val)
        {
            Destroy_();
            Init_(val);
        }
        return *this;
    }

    json_view& operator=(json_view&& val) noexcept
    {
        if (this !=&val)
        {
            swap(val);
        }
        return *this;
    }

    const json_view& operator[](std::size_t i) const
    {
        return at(i);
    }

    const json_view& operator[](const string_view_type& name) const
    {
        return at(name);
    }

    template <class SAllocator=std::allocator<char_type>>
    void dump(std::basic_string<char_type,char_traits_type,SAllocator>& s, 
              indenting line_indent = indenting::no_indent) const
    {
        std::error_code ec;

        dump(s, line_indent, ec);
        if (ec)
        {
            JSONCONS_THROW(ser_error(ec));
        }
    }

    template <class SAllocator=std::allocator<char_type>>
    void dump(std::basic_string<char_type,char_traits_type,SAllocator>& s,
              const basic_json_encode_options<char_type>& options, 
              indenting line_indent = indenting::no_indent) const
    {
        std::error_code ec;

        dump(s, options, line_indent, ec);
        if (ec)
        {
            JSONCONS_THROW(ser_error(ec));
        }
    }

    void dump(std::basic_ostream<char_type>& os, 
              indenting line_indent = indenting::no_indent) const
    {
        std::error_code ec;

        dump(os, line_indent, ec);
        if (ec)
        {
            JSONCONS_THROW(ser_error(ec));
        }
    }

    void dump(std::basic_ostream<char_type>& os, 
              const basic_json_encode_options<char_type>& options, 
              indenting line_indent = indenting::no_indent) const
    {
        std::error_code ec;

        dump(os, options, line_indent, ec);
        if (ec)
        {
            JSONCONS_THROW(ser_error(ec));
        }
    }

    void dump(basic_json_visitor<char_type>& visitor) const
    {
        std::error_code ec;
        dump(visitor, ec);
        if (ec)
        {
            JSONCONS_THROW(ser_error(ec));
        }
    }

    template <class SAllocator=std::allocator<char_type>>
    void dump(std::basic_string<char_type,char_traits_type,SAllocator>& s,
              const basic_json_encode_options<char_type>& options, 
              indenting line_indent,
              std::error_code& ec) const
    {
        using string_type = std::basic_string<char_type,char_traits_type,SAllocator>;
        if (line_indent == indenting::indent)
        {
            basic_json_encoder<char_type,jsoncons::string_sink<string_type>> encoder(s, options);
            dump(encoder, ec);
        }
        else
        {
            basic_compact_json_encoder<char_type,jsoncons::string_sink<string_type>> encoder(s, options);
            dump(encoder, ec);
        }
    }

    template <class SAllocator=std::allocator<char_type>>
    void dump(std::basic_string<char_type,char_traits_type,SAllocator>& s, 
              indenting line_indent,
              std::error_code& ec) const
    {
        using string_type = std::basic_string<char_type,char_traits_type,SAllocator>;
        if (line_indent == indenting::indent)
        {
            basic_json_encoder<char_type,jsoncons::string_sink<string_type>> encoder(s);
            dump(encoder, ec);
        }
        else
        {
            basic_compact_json_encoder<char_type,jsoncons::string_sink<string_type>> encoder(s);
            dump(encoder, ec);
        }
    }

    void dump(std::basic_ostream<char_type>& os, 
              const basic_json_encode_options<char_type>& options, 
              indenting line_indent,
              std::error_code& ec) const
    {
        if (line_indent == indenting::indent)
        {
            basic_json_encoder<char_type> encoder(os, options);
            dump(encoder, ec);
        }
        else
        {
            basic_compact_json_encoder<char_type> encoder(os, options);
            dump(encoder, ec);
        }
    }

    void dump(std::basic_ostream<char_type>& os, 
              indenting line_indent,
              std::error_code& ec) const
    {
        if (line_indent == indenting::indent)
        {
            basic_json_encoder<char_type> encoder(os);
            dump(encoder, ec);
        }
        else
        {
            basic_compact_json_encoder<char_type> encoder(os);
            dump(encoder, ec);
        }
    }

    void dump(basic_json_visitor<char_type>& visitor, 
              std::error_code& ec) const
    {
        dump_noflush(visitor, ec);
        if (ec)
        {
            return;
        }
        visitor.flush();
    }

    bool is_null() const noexcept
    {
        switch (storage())
        {
            case view_storage_kind::null_value:
                return true;
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->is_null();
            default:
                return false;
        }
    }

    allocator_type get_allocator() const
    {
        switch (storage())
        {
            case view_storage_kind::array_value:
            {
                return cast<array_storage>().get_allocator();
            }
            case view_storage_kind::object_value:
            {
                return cast<object_storage>().get_allocator();
            }
            default:
                return allocator_type();
        }
    }

    uint64_t ext_tag() const
    {
        switch (storage())
        {
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->ext_tag();
            default:
                return 0;
        }
    }

    bool contains(const string_view_type& key) const noexcept
    {
        switch (storage())
        {
            case view_storage_kind::object_value:
            {
                auto it = object_value().find(key);
                return it != object_value().end();
            }
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->contains(key);
            default:
                return false;
        }
    }

    std::size_t count(const string_view_type& key) const
    {
        switch (storage())
        {
            case view_storage_kind::object_value:
            {
                auto it = object_value().find(key);
                if (it == object_value().end())
                {
                    return 0;
                }
                std::size_t count = 0;
                while (it != object_value().end()&& it->key() == key)
                {
                    ++count;
                    ++it;
                }
                return count;
            }
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->count(key);
            default:
                return 0;
        }
    }

    template<class T, class... Args>
    bool is(Args&&... args) const noexcept
    {
        return json_type_traits<json_view,T>::is(*this,std::forward<Args>(args)...);
    }

    bool is_string() const noexcept
    {
        switch (storage())
        {
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->is_string();
            default:
                return false;
        }
    }

    bool is_string_view() const noexcept
    {
        return is_string();
    }

    bool is_byte_string() const noexcept
    {
        switch (storage())
        {
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->is_byte_string();
            default:
                return false;
        }
    }

    bool is_byte_string_view() const noexcept
    {
        return is_byte_string();
    }

    bool is_bignum() const
    {
        switch (storage())
        {
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->is_bignum();
            default:
                return false;
        }
    }

    bool is_bool() const noexcept
    {
        switch (storage())
        {
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->is_bool();
            default:
                return false;
        }
    }

    bool is_object() const noexcept
    {
        switch (storage())
        {
            case view_storage_kind::empty_object_value:
            case view_storage_kind::object_value:
                return true;
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->is_object();
            default:
                return false;
        }
    }

    bool is_array() const noexcept
    {
        switch (storage())
        {
            case view_storage_kind::array_value:
                return true;
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->is_array();
            default:
                return false;
        }
    }

    bool is_int64() const noexcept
    {
        switch (storage())
        {
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->is_int64();
            default:
                return false;
        }
    }

    bool is_uint64() const noexcept
    {
        switch (storage())
        {
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->is_uint64();
            default:
                return false;
        }
    }

    bool is_half() const noexcept
    {
        switch (storage())
        {
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->is_half();
            default:
                return false;
        }
    }

    bool is_double() const noexcept
    {
        switch (storage())
        {
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->is_double();
            default:
                return false;
        }
    }

    bool is_number() const noexcept
    {
        switch (storage())
        {
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->is_number();
            default:
                return false;
        }
    }

    bool empty() const noexcept
    {
        switch (storage())
        {
            case view_storage_kind::array_value:
                return array_value().size() == 0;
            case view_storage_kind::empty_object_value:
                return true;
            case view_storage_kind::object_value:
                return object_value().size() == 0;
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->empty();
            default:
                return false;
        }
    }

    std::size_t capacity() const
    {
        switch (storage())
        {
            case view_storage_kind::array_value:
                return array_value().capacity();
            case view_storage_kind::object_value:
                return object_value().capacity();
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->capacity();
            default:
                return 0;
        }
    }

    template<class U=allocator_type>
    void create_object_implicitly()
    {
        create_object_implicitly(jsoncons::detail::is_stateless<U>());
    }

    void create_object_implicitly(std::false_type)
    {
        static_assert(std::true_type::value, "Cannot create object implicitly - alloc is stateful.");
    }

    void create_object_implicitly(std::true_type)
    {
        *this = json_view(object(allocator_type()), tag());
    }

    void reserve(std::size_t n)
    {
        if (n > 0)
        {
            switch (storage())
            {
                case view_storage_kind::array_value:
                    array_value().reserve(n);
                    break;
                case view_storage_kind::empty_object_value:
                {
                    create_object_implicitly();
                    object_value().reserve(n);
                }
                break;
                case view_storage_kind::object_value:
                {
                    object_value().reserve(n);
                }
                    break;
                default:
                    break;
            }
        }
    }

    void resize(std::size_t n)
    {
        switch (storage())
        {
            case view_storage_kind::array_value:
                array_value().resize(n);
                break;
            default:
                break;
        }
    }

    template <class T>
    void resize(std::size_t n, T val)
    {
        switch (storage())
        {
            case view_storage_kind::array_value:
                array_value().resize(n, val);
                break;
            default:
                break;
        }
    }

    template<class T>
    typename std::enable_if<is_json_type_traits_specialized<json_view,T>::value,T>::type
    as() const
    {
        T val = json_type_traits<json_view,T>::as(*this);
        return val;
    }

    template<class T>
    typename std::enable_if<(!jsoncons::detail::is_basic_string<T>::value && 
                             jsoncons::detail::is_back_insertable_byte_container<T>::value) ||
                            is_basic_byte_string<T>::value,T>::type
    as(byte_string_arg_t, semantic_tag hint) const
    {
        converter<T> convert;
        std::error_code ec;
        switch (storage())
        {
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->template as<T>(byte_string_arg, hint);
            default:
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Not a byte string"));
        }
    }

    bool as_bool() const 
    {
        switch (storage())
        {
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->as_bool();
            default:
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Not a bool"));
        }
    }

    template <class T>
    T as_integer() const
    {
        switch (storage())
        {
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->template as_integer<T>();
            default:
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Not an integer"));
        }
    }

    double as_double() const
    {
        switch (storage())
        {
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->as_double();
            default:
                JSONCONS_THROW(json_runtime_error<std::invalid_argument>("Not a double"));
        }
    }

    template <class SAllocator=std::allocator<char_type>>
    std::basic_string<char_type,char_traits_type,SAllocator> as_string() const 
    {
        return as_string(SAllocator());
    }

    template <class SAllocator=std::allocator<char_type>>
    std::basic_string<char_type,char_traits_type,SAllocator> as_string(const SAllocator& alloc) const 
    {
        using string_type = std::basic_string<char_type,char_traits_type,SAllocator>;

        converter<string_type> convert;
        std::error_code ec;
        switch (storage())
        {
            case view_storage_kind::array_value:
            {
                string_type s(alloc);
                {
                    basic_compact_json_encoder<char_type,jsoncons::string_sink<string_type>> encoder(s);
                    dump(encoder);
                }
                return s;
            }
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->as_string(alloc);
            default:
            {
                string_type s(alloc);
                basic_compact_json_encoder<char_type,jsoncons::string_sink<string_type>> encoder(s);
                dump(encoder);
                return s;
            }
        }
    }

    const char_type* as_cstring() const
    {
        switch (storage())
        {
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->as_cstring();
            default:
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Not a cstring"));
        }
    }

    json_view& at(const string_view_type& name)
    {
        switch (storage())
        {
            case view_storage_kind::empty_object_value:
                JSONCONS_THROW(key_not_found(name.data(),name.length()));
            case view_storage_kind::object_value:
            {
                auto it = object_value().find(name);
                if (it == object_value().end())
                {
                    JSONCONS_THROW(key_not_found(name.data(),name.length()));
                }
                return it->value();
            }
            default:
            {
                JSONCONS_THROW(not_an_object(name.data(),name.length()));
            }
        }
    }

    const Json& at(const string_view_type& key) const
    {
        switch (storage())
        {
            case view_storage_kind::empty_object_value:
                JSONCONS_THROW(key_not_found(key.data(),key.length()));
            case view_storage_kind::object_value:
            {
                auto it = object_value().find(key);
                if (it == object_value().end())
                {
                    JSONCONS_THROW(key_not_found(key.data(),key.length()));
                }
                return it->value();
            }
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->at(key);
            default:
            {
                JSONCONS_THROW(not_an_object(key.data(),key.length()));
            }
        }
    }

    json_view& at(std::size_t i)
    {
        switch (storage())
        {
            case view_storage_kind::array_value:
                if (i >= array_value().size())
                {
                    JSONCONS_THROW(json_runtime_error<std::out_of_range>("Invalid array subscript"));
                }
                return array_value().operator[](i);
            case view_storage_kind::object_value:
                return object_value().at(i);
            default:
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Index on non-array value not supported"));
        }
    }

    const json_view& at(std::size_t i) const
    {
        switch (storage())
        {
            case view_storage_kind::array_value:
                if (i >= array_value().size())
                {
                    JSONCONS_THROW(json_runtime_error<std::out_of_range>("Invalid array subscript"));
                }
                return array_value().operator[](i);
            case view_storage_kind::object_value:
                return object_value().at(i);
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->at(i);
            default:
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Index on non-array value not supported"));
        }
    }

    object_iterator find(const string_view_type& name)
    {
        switch (storage())
        {
            case view_storage_kind::empty_object_value:
                return object_range().end();
            case view_storage_kind::object_value:
                return object_iterator(object_value().find(name));
            default:
            {
                JSONCONS_THROW(not_an_object(name.data(),name.length()));
            }
        }
    }

    const_object_iterator find(const string_view_type& key) const
    {
        switch (storage())
        {
            case view_storage_kind::empty_object_value:
                return object_range().end();
            case view_storage_kind::object_value:
                return const_object_iterator(object_value().find(key));
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->find(key);
            default:
            {
                JSONCONS_THROW(not_an_object(key.data(),key.length()));
            }
        }
    }

    const json_view& at_or_null(const string_view_type& key) const
    {
        switch (storage())
        {
            case view_storage_kind::null_value:
            case view_storage_kind::empty_object_value:
            {
                return null();
            }
            case view_storage_kind::object_value:
            {
                auto it = object_value().find(key);
                if (it != object_value().end())
                {
                    return it->value();
                }
                else
                {
                    return null();
                }
            }
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->at_or_null(key);
            default:
            {
                JSONCONS_THROW(not_an_object(key.data(),key.length()));
            }
        }
    }

    template <class T,class U>
    T get_value_or(const string_view_type& key, U&& default_value) const
    {
        static_assert(std::is_copy_constructible<T>::value,
                      "get_value_or: T must be copy constructible");
        static_assert(std::is_convertible<U&&, T>::value,
                      "get_value_or: U must be convertible to T");
        switch (storage())
        {
            case view_storage_kind::null_value:
            case view_storage_kind::empty_object_value:
            {
                return static_cast<T>(std::forward<U>(default_value));
            }
            case view_storage_kind::object_value:
            {
                auto it = object_value().find(key);
                if (it != object_value().end())
                {
                    return it->value().template as<T>();
                }
                else
                {
                    return static_cast<T>(std::forward<U>(default_value));
                }
            }
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->template get_value_or<T,U>(key,std::forward<U>(default_value));
            default:
            {
                JSONCONS_THROW(not_an_object(key.data(),key.length()));
            }
        }
    }

    // Modifiers

    void shrink_to_fit()
    {
        switch (storage())
        {
        case view_storage_kind::array_value:
            array_value().shrink_to_fit();
            break;
        case view_storage_kind::object_value:
            object_value().shrink_to_fit();
            break;
        default:
            break;
        }
    }

    void clear()
    {
        switch (storage())
        {
        case view_storage_kind::array_value:
            array_value().clear();
            break;
        case view_storage_kind::object_value:
            object_value().clear();
            break;
        default:
            break;
        }
    }

    void erase(const_object_iterator pos)
    {
        switch (storage())
        {
        case view_storage_kind::empty_object_value:
            break;
        case view_storage_kind::object_value:
            object_value().erase(pos);
            break;
        default:
            JSONCONS_THROW(json_runtime_error<std::domain_error>("Not an object"));
            break;
        }
    }

    void erase(const_object_iterator first, const_object_iterator last)
    {
        switch (storage())
        {
        case view_storage_kind::empty_object_value:
            break;
        case view_storage_kind::object_value:
            object_value().erase(first, last);
            break;
        default:
            JSONCONS_THROW(json_runtime_error<std::domain_error>("Not an object"));
            break;
        }
    }

    void erase(const_array_iterator pos)
    {
        switch (storage())
        {
        case view_storage_kind::array_value:
            array_value().erase(pos);
            break;
        default:
            JSONCONS_THROW(json_runtime_error<std::domain_error>("Not an array"));
            break;
        }
    }

    void erase(const_array_iterator first, const_array_iterator last)
    {
        switch (storage())
        {
        case view_storage_kind::array_value:
            array_value().erase(first, last);
            break;
        default:
            JSONCONS_THROW(json_runtime_error<std::domain_error>("Not an array"));
            break;
        }
    }

    // Removes all elements from an array value whose index is between from_index, inclusive, and to_index, exclusive.

    void erase(const string_view_type& name)
    {
        switch (storage())
        {
        case view_storage_kind::empty_object_value:
            break;
        case view_storage_kind::object_value:
            object_value().erase(name);
            break;
        default:
            JSONCONS_THROW(not_an_object(name.data(),name.length()));
            break;
        }
    }

    template <class T>
    std::pair<object_iterator,bool> insert_or_assign(const string_view_type& name, T&& val)
    {
        switch (storage())
        {
        case view_storage_kind::empty_object_value:
            create_object_implicitly();
            JSONCONS_FALLTHROUGH;
        case view_storage_kind::object_value:
        {
            auto result = object_value().insert_or_assign(name, std::forward<T>(val));
            return std::make_pair(object_iterator(result.first), result.second);
        }
        default:
            {
                JSONCONS_THROW(not_an_object(name.data(),name.length()));
            }
        }
    }

    template <class ... Args>
    std::pair<object_iterator,bool> try_emplace(const string_view_type& name, Args&&... args)
    {
        switch (storage())
        {
        case view_storage_kind::empty_object_value:
            create_object_implicitly();
            JSONCONS_FALLTHROUGH;
        case view_storage_kind::object_value:
        {
            auto result = object_value().try_emplace(name, std::forward<Args>(args)...);
            return std::make_pair(object_iterator(result.first),result.second);
        }
        default:
            {
                JSONCONS_THROW(not_an_object(name.data(),name.length()));
            }
        }
    }

    // merge

    void merge(const json_view& source)
    {
        switch (storage())
        {
        case view_storage_kind::empty_object_value:
            create_object_implicitly();
            JSONCONS_FALLTHROUGH;
        case view_storage_kind::object_value:
            object_value().merge(source.object_value());
            break;
        default:
            {
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Attempting to merge a value that is not an object"));
            }
        }
    }

    void merge(json_view&& source)
    {
        switch (storage())
        {
        case view_storage_kind::empty_object_value:
            create_object_implicitly();
            JSONCONS_FALLTHROUGH;
        case view_storage_kind::object_value:
            object_value().merge(std::move(source.object_value()));
            break;
        default:
            {
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Attempting to merge a value that is not an object"));
            }
        }
    }

    void merge(object_iterator hint, const json_view& source)
    {
        switch (storage())
        {
        case view_storage_kind::empty_object_value:
            create_object_implicitly();
            JSONCONS_FALLTHROUGH;
        case view_storage_kind::object_value:
            object_value().merge(hint, source.object_value());
            break;
        default:
            {
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Attempting to merge a value that is not an object"));
            }
        }
    }

    void merge(object_iterator hint, json_view&& source)
    {
        switch (storage())
        {
        case view_storage_kind::empty_object_value:
            create_object_implicitly();
            JSONCONS_FALLTHROUGH;
        case view_storage_kind::object_value:
            object_value().merge(hint, std::move(source.object_value()));
            break;
        default:
            {
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Attempting to merge a value that is not an object"));
            }
        }
    }

    // merge_or_update

    void merge_or_update(const json_view& source)
    {
        switch (storage())
        {
        case view_storage_kind::empty_object_value:
            create_object_implicitly();
            JSONCONS_FALLTHROUGH;
        case view_storage_kind::object_value:
            object_value().merge_or_update(source.object_value());
            break;
        default:
            {
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Attempting to merge or update a value that is not an object"));
            }
        }
    }

    void merge_or_update(json_view&& source)
    {
        switch (storage())
        {
        case view_storage_kind::empty_object_value:
            create_object_implicitly();
            JSONCONS_FALLTHROUGH;
        case view_storage_kind::object_value:
            object_value().merge_or_update(std::move(source.object_value()));
            break;
        default:
            {
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Attempting to merge or update a value that is not an object"));
            }
        }
    }

    void merge_or_update(object_iterator hint, const json_view& source)
    {
        switch (storage())
        {
        case view_storage_kind::empty_object_value:
            create_object_implicitly();
            JSONCONS_FALLTHROUGH;
        case view_storage_kind::object_value:
            object_value().merge_or_update(hint, source.object_value());
            break;
        default:
            {
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Attempting to merge or update a value that is not an object"));
            }
        }
    }

    void merge_or_update(object_iterator hint, json_view&& source)
    {
        switch (storage())
        {
        case view_storage_kind::empty_object_value:
            create_object_implicitly();
            JSONCONS_FALLTHROUGH;
        case view_storage_kind::object_value:
            object_value().merge_or_update(hint, std::move(source.object_value()));
            break;
        default:
            {
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Attempting to merge or update a value that is not an object"));
            }
        }
    }

    template <class T>
    object_iterator insert_or_assign(object_iterator hint, const string_view_type& name, T&& val)
    {
        switch (storage())
        {
        case view_storage_kind::empty_object_value:
            create_object_implicitly();
            JSONCONS_FALLTHROUGH;
        case view_storage_kind::object_value:
            return object_iterator(object_value().insert_or_assign(hint, name, std::forward<T>(val)));
        default:
            {
                JSONCONS_THROW(not_an_object(name.data(),name.length()));
            }
        }
    }

    template <class ... Args>
    object_iterator try_emplace(object_iterator hint, const string_view_type& name, Args&&... args)
    {
        switch (storage())
        {
        case view_storage_kind::empty_object_value:
            create_object_implicitly();
            JSONCONS_FALLTHROUGH;
        case view_storage_kind::object_value:
            return object_iterator(object_value().try_emplace(hint, name, std::forward<Args>(args)...));
        default:
            {
                JSONCONS_THROW(not_an_object(name.data(),name.length()));
            }
        }
    }

    template <class T>
    array_iterator insert(const_array_iterator pos, T&& val)
    {
        switch (storage())
        {
        case view_storage_kind::array_value:
            return array_value().insert(pos, std::forward<T>(val));
            break;
        default:
            {
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Attempting to insert into a value that is not an array"));
            }
        }
    }

    template <class InputIt>
    array_iterator insert(const_array_iterator pos, InputIt first, InputIt last)
    {
        switch (storage())
        {
        case view_storage_kind::array_value:
            return array_value().insert(pos, first, last);
            break;
        default:
            {
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Attempting to insert into a value that is not an array"));
            }
        }
    }

    template <class InputIt>
    void insert(InputIt first, InputIt last)
    {
        switch (storage())
        {
        case view_storage_kind::empty_object_value:
        case view_storage_kind::object_value:
            object_value().insert(first, last, get_key_value<key_type,json_view>());
            break;
        default:
            {
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Attempting to insert into a value that is not an object"));
            }
        }
    }

    template <class InputIt>
    void insert(sorted_unique_range_tag tag, InputIt first, InputIt last)
    {
        switch (storage())
        {
        case view_storage_kind::empty_object_value:
        case view_storage_kind::object_value:
            object_value().insert(tag, first, last, get_key_value<key_type,json_view>());
            break;
        default:
            {
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Attempting to insert into a value that is not an object"));
            }
        }
    }

    template <class... Args> 
    array_iterator emplace(const_array_iterator pos, Args&&... args)
    {
        switch (storage())
        {
        case view_storage_kind::array_value:
            return array_value().emplace(pos, std::forward<Args>(args)...);
            break;
        default:
            {
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Attempting to insert into a value that is not an array"));
            }
        }
    }

    template <class... Args> 
    json_view& emplace_back(Args&&... args)
    {
        switch (storage())
        {
        case view_storage_kind::array_value:
            return array_value().emplace_back(std::forward<Args>(args)...);
        default:
            {
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Attempting to insert into a value that is not an array"));
            }
        }
    }

    friend void swap(json_view& a, json_view& b) noexcept
    {
        a.swap(b);
    }

    template <class T>
    void push_back(T&& val)
    {
        switch (storage())
        {
        case view_storage_kind::array_value:
            array_value().push_back(std::forward<T>(val));
            break;
        default:
            {
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Attempting to insert into a value that is not an array"));
            }
        }
    }

    template<class T>
    T get_with_default(const string_view_type& key, const T& default_value) const
    {
        switch (storage())
        {
            case view_storage_kind::null_value:
            case view_storage_kind::empty_object_value:
            {
                return default_value;
            }
            case view_storage_kind::object_value:
            {
                auto it = object_value().find(key);
                if (it != object_value().end())
                {
                    return it->value().template as<T>();
                }
                else
                {
                    return default_value;
                }
            }
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->get_with_default(key, default_value);
            default:
            {
                JSONCONS_THROW(not_an_object(key.data(),key.length()));
            }
        }
    }

    template<class T = std::basic_string<char_type>>
    T get_with_default(const string_view_type& key, const char_type* default_value) const
    {
        switch (storage())
        {
            case view_storage_kind::null_value:
            case view_storage_kind::empty_object_value:
            {
                return T(default_value);
            }
            case view_storage_kind::object_value:
            {
                auto it = object_value().find(key);
                if (it != object_value().end())
                {
                    return it->value().template as<T>();
                }
                else
                {
                    return T(default_value);
                }
            }
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->get_with_default(key, default_value);
            default:
            {
                JSONCONS_THROW(not_an_object(key.data(),key.length()));
            }
        }
    }

    std::basic_string<char_type> to_string() const noexcept
    {
        using string_type = std::basic_string<char_type>;
        string_type s;
        basic_compact_json_encoder<char_type, jsoncons::string_sink<string_type>> encoder(s);
        dump(encoder);
        return s;
    }

    range<const_object_iterator, const_object_iterator> object_range() const
    {
        switch (storage())
        {
            case view_storage_kind::empty_object_value:
                return range<const_object_iterator, const_object_iterator>(const_object_iterator(), const_object_iterator());
            case view_storage_kind::object_value:
                return range<const_object_iterator, const_object_iterator>(const_object_iterator(object_value().begin()),
                                                    const_object_iterator(object_value().end()));
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->object_range();
            default:
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Not an object"));
        }
    }

    range<const_array_iterator, const_array_iterator> array_range() const
    {
        switch (storage())
        {
            case view_storage_kind::array_value:
                return range<const_array_iterator, const_array_iterator>(array_value().begin(),array_value().end());
            case view_storage_kind::json_const_pointer:
                cast<json_const_pointer_storage>().value()->array_range();
                //return cast<json_const_pointer_storage>().value()->array_range();
            default:
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Not an array"));
        }
    }

    array& array_value() 
    {
        switch (storage())
        {
        case view_storage_kind::array_value:
            return cast<array_storage>().value();
        default:
            JSONCONS_THROW(json_runtime_error<std::domain_error>("Bad array cast"));
            break;
        }
    }

    const array& array_value() const
    {
        switch (storage())
        {
            case view_storage_kind::array_value:
                return cast<array_storage>().value();
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->array_value();
            default:
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Bad array cast"));
                break;
        }
    }

    object& object_value()
    {
        switch (storage())
        {
            case view_storage_kind::empty_object_value:
                create_object_implicitly();
                JSONCONS_FALLTHROUGH;
            case view_storage_kind::object_value:
                return cast<object_storage>().value();
            default:
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Bad object cast"));
                break;
        }
    }

    const object& object_value() const
    {
        switch (storage())
        {
            case view_storage_kind::empty_object_value:
                const_cast<json_view*>(this)->create_object_implicitly(); // HERE
                JSONCONS_FALLTHROUGH;
            case view_storage_kind::object_value:
                return cast<object_storage>().value();
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->object_value();
            default:
                JSONCONS_THROW(json_runtime_error<std::domain_error>("Bad object cast"));
                break;
        }
    }

private:

    void dump_noflush(basic_json_visitor<char_type>& visitor, std::error_code& ec) const
    {
        const ser_context context{};
        switch (storage())
        {
            case view_storage_kind::null_value:
                visitor.null_value(tag(), context, ec);
                break;
            case view_storage_kind::empty_object_value:
                visitor.begin_object(0, tag(), context, ec);
                visitor.end_object(context, ec);
                break;
            case view_storage_kind::object_value:
            {
                bool more = visitor.begin_object(size(), tag(), context, ec);
                const object& o = object_value();
                for (auto it = o.begin(); more && it != o.end(); ++it)
                {
                    visitor.key(string_view_type((it->key()).data(),it->key().length()), context, ec);
                    it->value().dump_noflush(visitor, ec);
                }
                if (more)
                {
                    visitor.end_object(context, ec);
                }
                break;
            }
            case view_storage_kind::array_value:
            {
                bool more = visitor.begin_array(size(), tag(), context, ec);
                const array& o = array_value();
                for (const_array_iterator it = o.begin(); more && it != o.end(); ++it)
                {
                    it->dump_noflush(visitor, ec);
                }
                if (more)
                {
                    visitor.end_array(context, ec);
                }
                break;
            }
            case view_storage_kind::json_const_pointer:
                return cast<json_const_pointer_storage>().value()->dump_noflush(visitor, ec);
            default:
                break;
        }
    }
};

} // namespace jsoncons

#endif
