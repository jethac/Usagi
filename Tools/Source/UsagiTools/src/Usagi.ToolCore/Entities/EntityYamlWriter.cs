using System.Text;

namespace Usagi.ToolCore.Entities;

public sealed class EntityYamlWriter
{
    private readonly StringBuilder _output = new();
    private int _indent;

    public string Write(EditableEntityNode entity)
    {
        _output.Clear();
        _indent = 0;

        WriteEntity(entity, isRoot: true);

        return _output.ToString();
    }

    private void WriteEntity(EditableEntityNode entity, bool isRoot)
    {
        // Write Inherits first if present
        if (entity.Inherits.Count > 0)
        {
            WriteLine("Inherits:");
            foreach (var inherit in entity.Inherits)
            {
                WriteLine($"  - {inherit}");
            }
        }

        // Write components (including Identifier)
        foreach (var component in entity.Components)
        {
            WriteComponent(component);
        }

        // Write Overrides if present
        if (entity.Overrides.Count > 0)
        {
            WriteLine("Overrides:");
            _indent++;
            foreach (var ovr in entity.Overrides)
            {
                WriteLine($"- EntityWithID: {ovr.TargetEntityId}");
                _indent++;
                foreach (var (componentName, fields) in ovr.ComponentOverrides)
                {
                    WriteComponentFields(componentName, fields);
                }
                _indent--;
            }
            _indent--;
        }

        // Write Children if present
        if (entity.Children.Count > 0)
        {
            WriteLine("Children:");
            _indent++;
            foreach (var child in entity.Children)
            {
                WriteChildEntity(child);
            }
            _indent--;
        }

        // Write InitializerEvents if present
        if (entity.InitializerEvents.Count > 0)
        {
            WriteLine("InitializerEvents:");
            _indent++;
            foreach (var evt in entity.InitializerEvents)
            {
                WriteInitializerEvent(evt);
            }
            _indent--;
        }
    }

    private void WriteChildEntity(EditableEntityNode child)
    {
        var isFirst = true;
        foreach (var component in child.Components)
        {
            if (isFirst)
            {
                _output.Append(Indent());
                _output.Append("- ");
                WriteComponentInline(component);
                isFirst = false;
            }
            else
            {
                _output.Append(Indent());
                _output.Append("  ");
                WriteComponentInline(component);
            }
        }

        if (child.Children.Count > 0)
        {
            _output.Append(Indent());
            _output.AppendLine("  Children:");
            _indent += 2;
            foreach (var grandchild in child.Children)
            {
                WriteChildEntity(grandchild);
            }
            _indent -= 2;
        }
    }

    private void WriteComponent(EditableComponent component)
    {
        if (component.Fields.Count == 0)
        {
            WriteLine($"{component.Name}:");
        }
        else
        {
            WriteComponentFields(component.Name, component.Fields);
        }
    }

    private void WriteComponentInline(EditableComponent component)
    {
        if (component.Fields.Count == 0)
        {
            _output.AppendLine($"{component.Name}:");
        }
        else
        {
            _output.AppendLine($"{component.Name}:");
            _indent += 2;
            foreach (var (key, value) in component.Fields)
            {
                WriteField(key, value);
            }
            _indent -= 2;
        }
    }

    private void WriteComponentFields(string name, Dictionary<string, object?> fields)
    {
        WriteLine($"{name}:");
        _indent++;
        foreach (var (key, value) in fields)
        {
            WriteField(key, value);
        }
        _indent--;
    }

    private void WriteField(string key, object? value)
    {
        switch (value)
        {
            case null:
                WriteLine($"{key}:");
                break;
            case string s:
                WriteLine($"{key}: {FormatString(s)}");
                break;
            case bool b:
                WriteLine($"{key}: {(b ? "true" : "false")}");
                break;
            case int or long or float or double or decimal:
                WriteLine($"{key}: {value}");
                break;
            case Dictionary<string, object?> nested:
                WriteLine($"{key}:");
                _indent++;
                foreach (var (nestedKey, nestedValue) in nested)
                {
                    WriteField(nestedKey, nestedValue);
                }
                _indent--;
                break;
            case IEnumerable<object?> list:
                WriteLine($"{key}:");
                _indent++;
                foreach (var item in list)
                {
                    if (item is Dictionary<string, object?> itemDict)
                    {
                        _output.Append(Indent());
                        _output.Append("- ");
                        var first = true;
                        foreach (var (itemKey, itemValue) in itemDict)
                        {
                            if (first)
                            {
                                _output.AppendLine($"{itemKey}: {FormatValue(itemValue)}");
                                first = false;
                            }
                            else
                            {
                                _output.Append(Indent());
                                _output.AppendLine($"  {itemKey}: {FormatValue(itemValue)}");
                            }
                        }
                    }
                    else
                    {
                        WriteLine($"- {FormatValue(item)}");
                    }
                }
                _indent--;
                break;
            default:
                WriteLine($"{key}: {value}");
                break;
        }
    }

    private void WriteInitializerEvent(Dictionary<string, object?> evt)
    {
        _output.Append(Indent());
        _output.Append("- ");

        var first = true;
        foreach (var (eventName, eventData) in evt)
        {
            if (first)
            {
                _output.AppendLine($"{eventName}:");
                first = false;
            }

            if (eventData is Dictionary<string, object?> fields)
            {
                _indent += 2;
                foreach (var (key, value) in fields)
                {
                    WriteField(key, value);
                }
                _indent -= 2;
            }
        }
    }

    private static string FormatValue(object? value)
    {
        return value switch
        {
            null => "",
            string s => FormatString(s),
            bool b => b ? "true" : "false",
            _ => value.ToString() ?? ""
        };
    }

    private static string FormatString(string s)
    {
        if (s.Contains('\n') || s.Contains(':') || s.Contains('#') || s.Contains('"') || s.StartsWith(' ') || s.EndsWith(' '))
        {
            return $"\"{s.Replace("\"", "\\\"")}\"";
        }
        return s;
    }

    private void WriteLine(string line)
    {
        _output.Append(Indent());
        _output.AppendLine(line);
    }

    private string Indent() => new(' ', _indent * 2);
}
