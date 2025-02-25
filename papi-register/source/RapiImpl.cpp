#include "pesapi.h"
#include "JSClassRegister.h"
#include <string.h>
#include <EASTL/vector.h>
#include <EASTL/allocator_malloc.h>

EXTERN_C_START

struct pesapi_type_info__
{
    const char* name;
    bool is_pointer;
    bool is_const;
    bool is_ref;
    bool is_primitive;
};

struct pesapi_signature_info__
{
    pesapi_type_info return_type;
    size_t parameter_count;
    pesapi_type_info parameter_types;
};

struct pesapi_property_descriptor__
{
    const char* name;
    bool is_static;
    pesapi_callback method;
    pesapi_callback getter;
    pesapi_callback setter;
    void* data0;
    void* data1;

    union
    {
        pesapi_type_info type_info;
        pesapi_signature_info signature_info;
    } info;
};

pesapi_type_info pesapi_alloc_type_infos(size_t count)
{
    auto ret = static_cast<pesapi_type_info__*>(malloc(sizeof(pesapi_type_info__) * count));
    memset(ret, 0, sizeof(pesapi_type_info__) * count);
    return ret;
}

void pesapi_set_type_info(
    pesapi_type_info type_infos, size_t index, const char* name, bool is_pointer, bool is_const, bool is_ref, bool is_primitive)
{
    type_infos[index] = {name, is_pointer, is_const, is_ref, is_primitive};
}

pesapi_signature_info pesapi_create_signature_info(
    pesapi_type_info return_type, size_t parameter_count, pesapi_type_info parameter_types)
{
    auto info = static_cast<pesapi_signature_info__*>(malloc(sizeof(pesapi_signature_info__)));
    memset(info, 0, sizeof(pesapi_signature_info__));
    info->return_type = return_type;
    info->parameter_count = parameter_count;
    info->parameter_types = parameter_types;
    return info;
}

pesapi_property_descriptor pesapi_alloc_property_descriptors(size_t count)
{
    auto ret = static_cast<pesapi_property_descriptor__*>(malloc(sizeof(pesapi_property_descriptor__) * count));
    memset(ret, 0, sizeof(pesapi_property_descriptor__) * count);
    return ret;
}

void pesapi_set_method_info(pesapi_property_descriptor properties, size_t index, const char* name, bool is_static,
    pesapi_callback method, void* data, pesapi_signature_info signature_info)
{
    properties[index].name = name;
    properties[index].is_static = is_static;
    properties[index].method = method;
    properties[index].data0 = data;
    properties[index].info.signature_info = signature_info;
}

void pesapi_set_property_info(pesapi_property_descriptor properties, size_t index, const char* name, bool is_static,
    pesapi_callback getter, pesapi_callback setter, void* getter_data, void* setter_data, pesapi_type_info type_info)
{
    properties[index].name = name;
    properties[index].is_static = is_static;
    properties[index].getter = getter;
    properties[index].setter = setter;
    properties[index].data0 = getter_data;
    properties[index].data1 = setter_data;
    properties[index].info.type_info = type_info;
}

static void free_property_descriptor(pesapi_property_descriptor properties, size_t property_count)
{
    for (size_t i = 0; i < property_count; i++)
    {
        pesapi_property_descriptor p = properties + i;
        if (p->getter != nullptr || p->setter != nullptr)
        {
            if (p->info.type_info)
            {
                free(p->info.type_info);
            }
        }
        else if (p->method != nullptr)
        {
            if (p->info.signature_info)
            {
                if (p->info.signature_info->return_type)
                {
                    free(p->info.signature_info->return_type);
                }
                if (p->info.signature_info->parameter_types)
                {
                    free(p->info.signature_info->parameter_types);
                }
                p->info.signature_info->~pesapi_signature_info__();
                free(p->info.signature_info);
            }
        }
    }
}

// set module name here during loading, set nullptr after module loaded
const char* GPesapiModuleName = nullptr;

void pesapi_define_class(const void* type_id, const void* super_type_id, const char* type_name, pesapi_constructor constructor,
    pesapi_finalize finalize, size_t property_count, pesapi_property_descriptor properties, void* data)
{
    puerts::JSClassDefinition classDef = JSClassEmptyDefinition;
    classDef.TypeId = type_id;
    classDef.SuperTypeId = super_type_id;
    puerts::PString ScriptNameWithModuleName = GPesapiModuleName == nullptr ? puerts::PString() : GPesapiModuleName;
    if (GPesapiModuleName)
    {
        ScriptNameWithModuleName += ".";
        ScriptNameWithModuleName += type_name;
        classDef.ScriptName = ScriptNameWithModuleName.c_str();
    }
    else
    {
        classDef.ScriptName = type_name;
    }
    classDef.Data = data;

    classDef.Initialize = constructor;
    classDef.Finalize = finalize;

    eastl::vector<puerts::JSFunctionInfo, eastl::allocator_malloc> p_methods;
    eastl::vector<puerts::JSFunctionInfo, eastl::allocator_malloc> p_functions;
    eastl::vector<puerts::JSPropertyInfo, eastl::allocator_malloc> p_properties;
    eastl::vector<puerts::JSPropertyInfo, eastl::allocator_malloc> p_variables;

    for (int i = 0; i < property_count; i++)
    {
        pesapi_property_descriptor p = properties + i;
        if (p->getter != nullptr || p->setter != nullptr)
        {
            if (p->is_static)
            {
                p_variables.push_back({p->name, p->getter, p->setter, p->data0, p->data1});
            }
            else
            {
                p_properties.push_back({p->name, p->getter, p->setter, p->data0, p->data1});
            }
        }
        else if (p->method != nullptr)
        {
            puerts::JSFunctionInfo finfo{p->name, p->method, p->data0};
            if (p->is_static)
            {
                p_functions.push_back(finfo);
            }
            else
            {
                p_methods.push_back(finfo);
            }
        }
    }

    free_property_descriptor(properties, property_count);

    p_methods.push_back(puerts::JSFunctionInfo());
    p_functions.push_back(puerts::JSFunctionInfo());
    p_properties.push_back(puerts::JSPropertyInfo());
    p_variables.push_back(puerts::JSPropertyInfo());

    classDef.Methods = p_methods.data();
    classDef.Functions = p_functions.data();
    classDef.Properties = p_properties.data();
    classDef.Variables = p_variables.data();

    puerts::RegisterJSClass(classDef);
}

void* pesapi_get_class_data(const void* type_id, bool force_load)
{
    auto clsDef = force_load ? puerts::LoadClassByID(type_id) : puerts::FindClassByID(type_id);
    return clsDef ? clsDef->Data : nullptr;
}

bool pesapi_trace_native_object_lifecycle(
    const void* type_id, pesapi_on_native_object_enter on_enter, pesapi_on_native_object_exit on_exit)
{
    return puerts::TraceObjectLifecycle(type_id, on_enter, on_exit);
}

void pesapi_on_class_not_found(pesapi_class_not_found_callback callback)
{
    puerts::OnClassNotFound(callback);
}

void pesapi_class_type_info(const char* proto_magic_id, const void* type_id, const void* constructor_info, const void* methods_info,
    const void* functions_info, const void* properties_info, const void* variables_info)
{
    if (strcmp(proto_magic_id, PUERTS_BINDING_PROTO_ID()) != 0)
    {
        return;
    }

    puerts::SetClassTypeInfo(type_id, static_cast<const puerts::NamedFunctionInfo*>(constructor_info),
        static_cast<const puerts::NamedFunctionInfo*>(methods_info), static_cast<const puerts::NamedFunctionInfo*>(functions_info),
        static_cast<const puerts::NamedPropertyInfo*>(properties_info),
        static_cast<const puerts::NamedPropertyInfo*>(variables_info));
}

const void* pesapi_find_type_id(const char* module_name, const char* type_name)
{
    puerts::PString fullname = module_name;
    fullname += ".";
    fullname += type_name;
    const auto class_def = puerts::FindCppTypeClassByName(fullname);
    return class_def ? class_def->TypeId : nullptr;
}

EXTERN_C_END

MSVC_PRAGMA(warning(push))
MSVC_PRAGMA(warning(disable : 4191))
pesapi_func_ptr reg_apis[] = {(pesapi_func_ptr) &pesapi_alloc_type_infos, (pesapi_func_ptr) &pesapi_set_type_info,
    (pesapi_func_ptr) &pesapi_create_signature_info, (pesapi_func_ptr) &pesapi_alloc_property_descriptors,
    (pesapi_func_ptr) &pesapi_set_method_info, (pesapi_func_ptr) &pesapi_set_property_info, (pesapi_func_ptr) &pesapi_define_class,
    (pesapi_func_ptr) &pesapi_get_class_data, (pesapi_func_ptr) &pesapi_trace_native_object_lifecycle,
    (pesapi_func_ptr) &pesapi_on_class_not_found, (pesapi_func_ptr) &pesapi_class_type_info,
    (pesapi_func_ptr) &pesapi_find_type_id};
MSVC_PRAGMA(warning(pop))
