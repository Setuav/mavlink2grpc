"""
Type converter: MAVLink types to Protocol Buffer types.
"""
from typing import Dict, Tuple


class TypeConverter:
    """Converts MAVLink types to Protocol Buffer types."""

    # MAVLink type → (Proto type, is_fixed_width)
    TYPE_MAP: Dict[str, Tuple[str, bool]] = {
        # Unsigned integers
        "uint8_t": ("uint32", False),
        "uint16_t": ("uint32", False),
        "uint32_t": ("uint32", False),
        "uint64_t": ("uint64", False),

        # Signed integers
        "int8_t": ("int32", False),
        "int16_t": ("int32", False),
        "int32_t": ("int32", False),
        "int64_t": ("int64", False),

        # Floating point
        "float": ("float", False),
        "double": ("double", False),

        # Character/string
        "char": ("string", False),

        # Special MAVLink types
        "uint8_t_mavlink_version": ("uint32", False),
    }

    @classmethod
    def to_proto_type(cls, mavlink_type: str) -> str:
        """
        Convert MAVLink type to Protocol Buffer type.

        Args:
            mavlink_type: MAVLink field type (e.g., "uint8_t", "char[16]", "float")

        Returns:
            Proto type (e.g., "uint32", "string", "float")

        Examples:
            >>> TypeConverter.to_proto_type("uint8_t")
            'uint32'
            >>> TypeConverter.to_proto_type("char[16]")
            'string'
            >>> TypeConverter.to_proto_type("uint8_t[8]")
            'bytes'
            >>> TypeConverter.to_proto_type("uint32_t[6]")
            'repeated uint32'
        """
        # Handle array types
        if "[" in mavlink_type and "]" in mavlink_type:
            base_type = mavlink_type[:mavlink_type.index("[")]

            # char[] arrays are strings in proto
            if base_type == "char":
                return "string"

            # uint8_t[] arrays become bytes (compact binary representation)
            if base_type == "uint8_t":
                return "bytes"

            # Other numeric arrays use repeated
            proto_type, _ = cls.TYPE_MAP.get(base_type, ("bytes", False))
            return f"repeated {proto_type}"

        # Look up in type map
        proto_type, _ = cls.TYPE_MAP.get(mavlink_type, ("bytes", False))
        return proto_type

    @classmethod
    def is_enum_type(cls, field_type: str) -> bool:
        """
        Check if a field type is an enum reference (not in type map).

        Args:
            field_type: MAVLink field type

        Returns:
            True if this is likely an enum type
        """
        # If not in type map and not an array, probably an enum
        return "[" not in field_type and field_type not in cls.TYPE_MAP

    @classmethod
    def get_field_type_info(cls, field_type: str, enum_name: str = None) -> dict:
        """
        Get detailed type information for C++ code generation.

        Args:
            field_type: MAVLink field type
            enum_name: Optional enum name if field references an enum

        Returns:
            Dict with: proto_type, cpp_type, is_enum, is_array, array_length
        """
        info = {
            'proto_type': cls.to_proto_type(field_type),
            'is_enum': enum_name is not None,
            'is_array': False,
            'array_length': None,
            'cpp_type': field_type
        }

        if '[' in field_type and ']' in field_type:
            info['is_array'] = True
            start = field_type.index('[')
            end = field_type.index(']')
            info['array_length'] = int(field_type[start + 1:end])
            info['cpp_type'] = field_type[:start]

        if enum_name:
            info['proto_type'] = cls.sanitize_enum_name(enum_name)

        if info['cpp_type'] == 'uint8_t_mavlink_version':
            info['cpp_type'] = 'uint8_t'

        return info

    @classmethod
    def sanitize_enum_name(cls, name: str) -> str:
        """
        Convert MAVLink enum name to Proto enum name (PascalCase).

        Args:
            name: MAVLink enum name (e.g., "MAV_TYPE", "MAV_AUTOPILOT")

        Returns:
            Proto enum name (e.g., "MavType", "MavAutopilot")

        Examples:
            >>> TypeConverter.sanitize_enum_name("MAV_TYPE")
            'MavType'
            >>> TypeConverter.sanitize_enum_name("MAV_MODE_FLAG")
            'MavModeFlag'
        """
        # Split by underscore and capitalize each part
        parts = name.split("_")
        return "".join(word.capitalize() for word in parts)

    @classmethod
    def sanitize_message_name(cls, name: str) -> str:
        """
        Convert MAVLink message name to Proto message name (PascalCase).

        Args:
            name: MAVLink message name (e.g., "HEARTBEAT", "GLOBAL_POSITION_INT")

        Returns:
            Proto message name (e.g., "Heartbeat", "GlobalPositionInt")

        Examples:
            >>> TypeConverter.sanitize_message_name("HEARTBEAT")
            'Heartbeat'
            >>> TypeConverter.sanitize_message_name("GLOBAL_POSITION_INT")
            'GlobalPositionInt'
        """
        # Split by underscore and capitalize each part
        parts = name.split("_")
        return "".join(word.capitalize() for word in parts)

    @classmethod
    def sanitize_field_name(cls, name: str) -> str:
        """
        Convert MAVLink field name to Proto field name (snake_case, lowercase).

        Args:
            name: MAVLink field name

        Returns:
            Proto field name (lowercase snake_case)

        Examples:
            >>> TypeConverter.sanitize_field_name("base_mode")
            'base_mode'
            >>> TypeConverter.sanitize_field_name("Vcc")
            'vcc'
            >>> TypeConverter.sanitize_field_name("Vservo")
            'vservo'
        """
        # Convert to lowercase for protobuf compatibility
        # Protobuf field names should be lowercase_with_underscores
        return name.lower()

    @classmethod
    def format_comment(cls, text: str, indent: int = 0) -> str:
        """
        Format a description as a proto comment.

        Args:
            text: Description text
            indent: Indentation level (spaces)

        Returns:
            Formatted comment with // prefix

        Examples:
            >>> TypeConverter.format_comment("This is a test", 2)
            '  // This is a test'
        """
        if not text:
            return ""

        # Clean up text (remove extra whitespace, newlines)
        text = " ".join(text.split())

        prefix = " " * indent

        # Split long comments into multiple lines (max 80 chars)
        max_line_length = 80 - indent - 3  # "// " takes 3 chars

        if len(text) <= max_line_length:
            return f"{prefix}// {text}"

        # Split into multiple lines
        words = text.split()
        lines = []
        current_line = []
        current_length = 0

        for word in words:
            word_length = len(word) + 1  # +1 for space
            if current_length + word_length > max_line_length:
                lines.append(" ".join(current_line))
                current_line = [word]
                current_length = word_length
            else:
                current_line.append(word)
                current_length += word_length

        if current_line:
            lines.append(" ".join(current_line))

        return "\n".join(f"{prefix}// {line}" for line in lines)
