#ifndef _V8_HANDLER_HPP_
#define _V8_HANDLER_HPP_

namespace function_types = boost::function_types;
namespace mpl = boost::mpl;

struct MyV8Handler : public CefV8Handler {

	typedef bool (CefV8Value::*CheckTypeFn)();
	typedef vector<CheckTypeFn> Validators;

	// Transform that removes the references from types in the sequence
	struct remove_ref { 
		template <class T>
		struct apply {
			typedef typename boost::remove_reference<T>::type type;
		};
	};

	template<int T> struct Int2Type {};

	struct AddValidatorFunctor {
		AddValidatorFunctor(Validators &v) : _validators(v) {}
		Validators &_validators;

		template<typename T> struct add_validator;
		template<> struct add_validator<int> { void add(Validators &v) { v.push_back(&CefV8Value::IsInt); } };
		template<> struct add_validator<bool> { void add(Validators &v) { v.push_back(&CefV8Value::IsBool); } };
		template<> struct add_validator<float> { void add(Validators &v) { v.push_back(&CefV8Value::IsDouble); } };
		template<> struct add_validator<double> { void add(Validators &v) { v.push_back(&CefV8Value::IsDouble); } };
		template<> struct add_validator<string> { void add(Validators &v) { v.push_back(&CefV8Value::IsString); } };
		template<> struct add_validator<CefString> { void add(Validators &v) { v.push_back(&CefV8Value::IsString); } };

		template <typename T>
		void operator()(T t) {
			add_validator<T> v;
			v.add(_validators);
		}
	};

	// unbox a CefV8Value to a native type
	template<typename T> struct unbox_value;
	template<> struct unbox_value<bool> { unbox_value(CefV8Value& v) : _v(v) {} CefV8Value& _v; operator bool() { return _v.GetBoolValue(); } };
	template<> struct unbox_value<int> { unbox_value(CefV8Value& v) : _v(v) {} CefV8Value& _v; operator int() { return _v.GetIntValue(); } };
	template<> struct unbox_value<float> { unbox_value(CefV8Value& v) : _v(v) {} CefV8Value& _v; operator float() { return (float)_v.GetDoubleValue(); } };
	template<> struct unbox_value<double> { unbox_value(CefV8Value& v) : _v(v) {} CefV8Value& _v; operator double() { return _v.GetDoubleValue(); } };
	template<> struct unbox_value<CefString> { unbox_value(CefV8Value& v) : _v(v) {} CefV8Value& _v; operator CefString() { return _v.GetStringValue(); } };

	// box a native type in a CefV8Value
	template<typename T> struct box_value;
	template<> struct box_value<bool> { box_value(bool v) : _v(v) {} bool _v; operator CefRefPtr<CefV8Value>() { return CefV8Value::CreateBool(_v); } };
	template<> struct box_value<int> { box_value(int v) : _v(v) {} int _v; operator CefRefPtr<CefV8Value>() { return CefV8Value::CreateInt(_v); } };
	template<> struct box_value<float> { box_value(float v) : _v(v) {} float _v; operator CefRefPtr<CefV8Value>() { return CefV8Value::CreateDouble(_v); } };
	template<> struct box_value<double> { box_value(double v) : _v(v) {} double _v; operator CefRefPtr<CefV8Value>() { return CefV8Value::CreateDouble(_v); } };
	template<> struct box_value<CefString> { box_value(CefString v) : _v(v) {} CefString _v; operator CefRefPtr<CefV8Value>() { return CefV8Value::CreateString(_v); } };

	struct Function {
		Function(const string &cef_name, const string &native_name, const string &args) : _active(true), _cef_name(cef_name), _native_name(native_name), _args(args) {}
		virtual bool validate(const CefV8ValueList& arguments) = 0;
		virtual CefRefPtr<CefV8Value> execute(const CefV8ValueList& arguments) = 0;
		bool _active;
		string _cef_name;
		string _native_name;
		string _args;
		Validators _validators;
	};


	template<class Prototype, class Callable>
	struct FunctionT : public Function {

		enum { IsMemberFunction = boost::is_member_function_pointer<Prototype>::value };
		typedef typename function_types::result_type<Prototype>::type ResultType;
		enum { IsVoid = boost::is_void<ResultType>::value };
		enum { Arity = function_types::function_arity<Prototype>::value - IsMemberFunction };

		// Remove the references from the parameter types
		typedef typename mpl::transform< function_types::parameter_types<Prototype>, remove_ref>::type ParamTypes1;

		// Remove the class from the parameter types if this is a member function pointer
		typedef typename mpl::if_<
			typename mpl::bool_<IsMemberFunction>::type,
			typename mpl::pop_front<ParamTypes1>::type,
			ParamTypes1>::type ParamTypes;

		FunctionT(const string &cef_name, const string &native_name, const string &args, const Callable &fn)
			: Function(cef_name, native_name, args), _fn(fn)
		{
		}

		virtual bool validate(const CefV8ValueList& arguments) {
			if (arguments.size() != _validators.size())
				return false;
			// Apply the validator for each argument to check it's of the correct type
			for (size_t i = 0; i < _validators.size(); ++i) {
				if (!((arguments[i].get())->*_validators[i])())
					return false;
			}
			return true;
		}

		// specialize the executors on the function arity
		template<class Arity, class ParamTypes, class ResultType, class IsVoid> struct exec;

		template<class ParamTypes, class ResultType> struct exec< Int2Type<0>, ParamTypes, ResultType, Int2Type<0> > {
			template<class Fn> CefRefPtr<CefV8Value> operator()(Fn& fn, const CefV8ValueList& arguments) {
				return box_value<ResultType>(fn());
			}
		};

		// unbox each parameter, call the function, and box the resulting value
		template<class ParamTypes, class ResultType> struct exec< Int2Type<1>, ParamTypes, ResultType, Int2Type<0> > {
			CefRefPtr<CefV8Value> operator()(Callable& fn, const CefV8ValueList& arguments) {
				return box_value<ResultType>(fn(
					unbox_value<mpl::at<ParamTypes, mpl::int_<0> >::type>(*arguments[0].get())));
			}
		};

		template<class ParamTypes, class ResultType> struct exec< Int2Type<2>, ParamTypes, ResultType, Int2Type<0>> {
			template<class Fn> CefRefPtr<CefV8Value> operator()(Fn& fn, const CefV8ValueList& arguments) {
				return box_value<ResultType>(fn(
					unbox_value<mpl::at<ParamTypes, mpl::int_<0> >::type>(*arguments[0].get()), 
					unbox_value<mpl::at<ParamTypes, mpl::int_<1> >::type>(*arguments[1].get())));
			}
		};

		template<class ParamTypes, class ResultType> struct exec< Int2Type<3>, ParamTypes, ResultType, Int2Type<0>> {
			template<class Fn> CefRefPtr<CefV8Value> operator()(Fn& fn, const CefV8ValueList& arguments) {
				return box_value<ResultType>(fn(
					unbox_value<mpl::at<ParamTypes, mpl::int_<0> >::type>(*arguments[0].get()), 
					unbox_value<mpl::at<ParamTypes, mpl::int_<1> >::type>(*arguments[1].get()), 
					unbox_value<mpl::at<ParamTypes, mpl::int_<2> >::type>(*arguments[2].get())));
			}
		};

		// void return value executors
		template<class ParamTypes, class ResultType> struct exec< Int2Type<0>, ParamTypes, ResultType, Int2Type<1> > {
			template<class Fn> CefRefPtr<CefV8Value> operator()(Fn& fn, const CefV8ValueList& arguments) {
				fn();
				return CefV8Value::CreateNull();
			}
		};

		template<class ParamTypes, class ResultType> struct exec< Int2Type<1>, ParamTypes, ResultType, Int2Type<1> > {
			template<class Fn> CefRefPtr<CefV8Value> operator()(Fn& fn, const CefV8ValueList& arguments) {
				fn(unbox_value<mpl::at<ParamTypes, mpl::int_<0> >::type>(*arguments[0].get()));
				return CefV8Value::CreateNull();
			}
		};

		template<class ParamTypes, class ResultType> struct exec< Int2Type<2>, ParamTypes, ResultType, Int2Type<1>> {
			template<class Fn> CefRefPtr<CefV8Value> operator()(Fn& fn, const CefV8ValueList& arguments) {
				fn(
					unbox_value<mpl::at<ParamTypes, mpl::int_<0> >::type>(*arguments[0].get()),
					unbox_value<mpl::at<ParamTypes, mpl::int_<1> >::type>(*arguments[1].get()));
				return CefV8Value::CreateNull();
			}
		};

		template<class ParamTypes, class ResultType> struct exec< Int2Type<3>, ParamTypes, ResultType, Int2Type<1>> {
			template<class Fn> CefRefPtr<CefV8Value> operator()(Fn& fn, const CefV8ValueList& arguments) {
				fn(
					unbox_value<mpl::at<ParamTypes, mpl::int_<0> >::type>(*arguments[0].get()),
					unbox_value<mpl::at<ParamTypes, mpl::int_<1> >::type>(*arguments[1].get()),
					unbox_value<mpl::at<ParamTypes, mpl::int_<2> >::type>(*arguments[2].get()));
				return CefV8Value::CreateNull();
			}
		};

		virtual CefRefPtr<CefV8Value> execute(const CefV8ValueList& arguments) {
			typedef exec<Int2Type<Arity>, ParamTypes, ResultType, Int2Type<IsVoid>> E;
			E e;
			return e(_fn, arguments);
		}

		Callable _fn;
	};

	virtual bool Execute(const CefString& name, CefRefPtr<CefV8Value> object, const CefV8ValueList& arguments, CefRefPtr<CefV8Value>& retval, CefString& exception)
	{
		Functions::iterator it_func = _functions.find(name);
		if (it_func != _functions.end()) {
			// got a function call
			Function *func = it_func->second;
			if (!func->_active || !func->validate(arguments))
				return false;

			retval = func->execute(arguments);
			return true;
		}

		// check for variable access
		const string str = name.ToString();
		const bool getter = begins_with(str.c_str(), "get_");
		const bool setter = begins_with(str.c_str(), "set_");

		if (!(getter || setter))
			return false;

		const string var_name(str.c_str() + 4, str.size() - 4);
		Variables::iterator it_var = _variables.find(var_name);
		if (it_var == _variables.end())
			return false;

		Variable *var = it_var->second;
		if (!var->_active)
			return false;

		if (getter)
			retval = var->get();
		else
			var->set((arguments[0]));

		return true;
	}

	template<typename Arity> struct get_param_string;
	template<> struct get_param_string<Int2Type<0>> { operator string() { return ""; } };
	template<> struct get_param_string<Int2Type<1>> { operator string() { return "a"; } };
	template<> struct get_param_string<Int2Type<2>> { operator string() { return "a,b"; } };
	template<> struct get_param_string<Int2Type<3>> { operator string() { return "a,b,c"; } };

	template <typename Fn> 
	void AddFunction(const char *cef_name, const char *native_name, Fn fn) {
		KASSERT(_functions.find(cef_name) == _functions.end());
		typedef FunctionT<Fn, Fn> F;
		Function *f = new F(cef_name, native_name, get_param_string<Int2Type<F::Arity>>(), fn);
		_functions[cef_name] = f;

		// create a validator for the function
		mpl::for_each<typename F::ParamTypes>(AddValidatorFunctor(f->_validators));
	}

	// add member function
	template <class Obj, typename MemFn> 
	void AddFunction(const char *cef_name, const char *native_name, Obj obj, MemFn fn) {
		KASSERT(_functions.find(cef_name) == _functions.end());
		auto bb = MakeDelegate(obj, fn);
		typedef FunctionT<MemFn, decltype(bb)> F;
		Function *f = new F(cef_name, native_name, get_param_string<Int2Type<F::Arity>>(), bb);
		_functions[cef_name] = f;
		// create a validator for the function
		mpl::for_each<typename F::ParamTypes>(AddValidatorFunctor(f->_validators));
	}

	struct Variable {
		Variable(const string &cef_name) : _active(true), _cef_name(cef_name) {}
		virtual bool set_is_valid(const CefV8Value &value) = 0;
		virtual CefRefPtr<CefV8Value> get() = 0;
		virtual void set(const CefRefPtr<CefV8Value> &value) = 0;
		bool _active;
		string _cef_name;
	};

	template<class T>
	struct VariableT : public Variable {
		VariableT(const string &cef_name, T *ptr) : Variable(cef_name), _ptr(ptr) {}
		virtual bool set_is_valid(const CefV8Value &value) {
			return false;
		}

		virtual CefRefPtr<CefV8Value> get() {
			return box_value<T>(*_ptr);
		}

		virtual void set(const CefRefPtr<CefV8Value> &value) {
			*_ptr = unbox_value<T>(*(value.get()));
		}

		T *_ptr;
	};

	void RemoveFunction(const char *cef_name) {
		Functions::iterator it = _functions.find(cef_name);
		if (it == _functions.end())
			return;
		it->second->_active = false;
	}

	void RemoveVariable(const char *cef_name) {
		Variables::iterator it = _variables.find(cef_name);
		if (it == _variables.end())
			return;
		it->second->_active = false;
	}

	template <typename T>
	void AddVariable(const char *cef_name, T* ptr) {
		KASSERT(_variables.find(cef_name) == _variables.end());
		Variable *v = new VariableT<T>(cef_name, ptr);
		_variables[cef_name] = v;
	}

	void Register()
	{
		string code = "var cef;"
			"if (!cef)\n"
			"  cef = {};\n"
			"if (!cef.test)\n"
			"  cef.test = {};\n";

		const char *var_template = ""
			"cef.test.__defineGetter__('%s', function() {\n"
			"  native function get_%s();\n"
			"  return get_%s();\n"
			"});\n"
			"cef.test.__defineSetter__('%s', function(b) {\n"
			"  native function set_%s();\n"
			"  if(b) set_%s(b);\n"
			"});\n";

		const char *fn_template = ""
			"cef.test.%s = function(%s) {\n"
			"  native function %s(%s);\n"
			"  return %s(%s);\n"
			"};\n";

		// use the template to create the function bindings
		for (auto it = _functions.begin(); it != _functions.end(); ++it) {
			const Function *cur = it->second;
			code += to_string(fn_template,
				cur->_cef_name.c_str(), cur->_args.c_str(), cur->_native_name.c_str(), cur->_args.c_str(), cur->_native_name.c_str(), cur->_args.c_str());
		}

		// create the variable bindings
		for (auto it = _variables.begin(); it != _variables.end(); ++it) {
			const Variable *cur = it->second;
			code += to_string(var_template,
				cur->_cef_name.c_str(), cur->_cef_name.c_str(), cur->_cef_name.c_str(), cur->_cef_name.c_str(), cur->_cef_name.c_str(), cur->_cef_name.c_str());
		}

		CefRegisterExtension("v8/test", code, this);
	}

	string _code;

	typedef map<string, Function *> Functions;
	Functions _functions;

	typedef map<string, Variable *> Variables;
	Variables _variables;

	IMPLEMENT_REFCOUNTING(MyV8Handler);
};

#endif
