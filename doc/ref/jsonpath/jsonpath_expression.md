### jsoncons::jsonpath::jsonpath_expression

```c++
#include <jsoncons_ext/jsonpath/jsonpath.hpp>

template <class Json,class JsonReference>
class jsonpath_expression
```

#### Member functions
```c++
Json evaluate(reference instance, result_options options = result_options::value); (1)
```
```c++
template <class BinaryCallback>
void evaluate(reference instance, BinaryCallback callback, 
              result_options options = result_options::value);  (2)
```

(1) Evaluates the Json value `root` against the compiled JSONPath expression and returns an array of values or 
normalized path expressions. 

(2) Evaluates the Json value `root` against the compiled JSONPath expression and calls a provided
callback repeatedly with the results.

#### Parameters

<table>
  <tr>
    <td>instance</td>
    <td>Json value</td> 
  </tr>
  <tr>
    <td><code>callback</code></td>
    <td>A function object that accepts a path and a reference to a Json value. 
It must have function call signature equivalent to
<br/><br/><code>
void fun(const Json::string_type& path, const Json& val);
</code><br/><br/>
  </tr>
  <tr>
    <td>result_options</td>
    <td>Result options, a bitmask of type <a href="result_options.md">result_options</></td> 
  </tr>
</table>

#### Static functions

    static jsonpath_expression compile(const string_view_type& expr);    (1)

    static jsonpath_expression compile(const string_view_type& expr,
                                       std::error_code& ec);             (2)

Compiles the JSONPath expression for later evaluation. Returns a `jsonpath_expression` object 
that represents the JSONPath expression.

#### Parameters

<table>
  <tr>
    <td>expr</td>
    <td>JSONPath expression</td> 
  </tr>
  <tr>
    <td>ec</td>
    <td>out-parameter for reporting errors in the non-throwing overload</td> 
  </tr>
</table>

#### Exceptions

(1) Throws a [jsonpath_error](jsonpath_error.md) if JSONPath compilation fails.

(2) Sets the out-parameter `ec` to the [jsonpath_error_category](jsonpath_errc.md) if JSONPath compilation fails. 

