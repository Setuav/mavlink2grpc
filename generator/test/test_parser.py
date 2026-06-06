#!/usr/bin/env python3
"""
Test script for MAVLink XML parser.
"""
import sys
from pathlib import Path

# Add parent directory to path to import src as a package
sys.path.insert(0, str(Path(__file__).parent.parent))

from src.parser import MAVLinkParser
from rich.console import Console
from rich.table import Table
from rich.panel import Panel

console = Console()


def main():
    """Test the parser with real MAVLink XML files."""
    # Path to MAVLink definitions (go up to project root, then to third_party/mavlink or mavlink)
    xml_dir = Path(__file__).parent.parent.parent / "third_party" / "mavlink" / "message_definitions" / "v1.0"
    if not xml_dir.exists():
        xml_dir = Path(__file__).parent.parent.parent / "mavlink" / "message_definitions" / "v1.0"

    if not xml_dir.exists():
        console.print(f"[red]Error: MAVLink definitions not found at {xml_dir}[/red]")
        return

    console.print(Panel.fit("MAVLink XML Parser Test", style="bold blue"))

    # Create parser
    parser = MAVLinkParser(xml_dir)

    # Test 1: Parse minimal.xml
    console.print("\n[bold cyan]Test 1: Parsing minimal.xml[/bold cyan]")
    minimal = parser.parse_file("minimal.xml")

    console.print(f"[OK] Dialect: {minimal.name}")
    console.print(f"[OK] Version: {minimal.version}")
    console.print(f"[OK] Enums: {len(minimal.enums)}")
    console.print(f"[OK] Messages: {len(minimal.messages)}")

    # Show HEARTBEAT message
    if 0 in minimal.messages:
        heartbeat = minimal.messages[0]
        console.print(f"\n[green]HEARTBEAT Message (ID={heartbeat.id}):[/green]")

        table = Table(title="HEARTBEAT Fields")
        table.add_column("Field", style="cyan")
        table.add_column("Type", style="magenta")
        table.add_column("Enum", style="yellow")
        table.add_column("Description", style="white")

        for field in heartbeat.fields:
            table.add_row(
                field.name,
                field.type,
                field.enum or "-",
                (field.description[:50] + "...") if field.description and len(field.description) > 50 else (field.description or "")
            )

        console.print(table)

    # Show MAV_TYPE enum
    if "MAV_TYPE" in minimal.enums:
        mav_type = minimal.enums["MAV_TYPE"]
        console.print(f"\n[green]MAV_TYPE Enum ({len(mav_type.entries)} entries):[/green]")

        table = Table(title="MAV_TYPE (first 10 entries)")
        table.add_column("Value", style="cyan")
        table.add_column("Name", style="magenta")
        table.add_column("Description", style="white")

        for entry in mav_type.entries[:10]:
            table.add_row(
                str(entry.value),
                entry.name,
                (entry.description[:40] + "...") if entry.description and len(entry.description) > 40 else (entry.description or "")
            )

        console.print(table)

    # Test 2: Parse common.xml (with includes)
    console.print("\n[bold cyan]Test 2: Parsing common.xml[/bold cyan]")
    common = parser.parse_file("common.xml")

    console.print(f"[OK] Dialect: {common.name}")
    console.print(f"[OK] Includes: {', '.join(common.includes)}")
    console.print(f"[OK] Enums: {len(common.enums)}")
    console.print(f"[OK] Messages: {len(common.messages)}")

    # Test 3: Resolve includes (merge common with its dependencies)
    console.print("\n[bold cyan]Test 3: Resolving includes for common.xml[/bold cyan]")
    merged_common = parser.resolve_includes("common")

    console.print(f"[OK] Merged dialect: {merged_common.name}")
    console.print(f"[OK] Total enums: {len(merged_common.enums)}")
    console.print(f"[OK] Total messages: {len(merged_common.messages)}")

    # Show some statistics
    console.print("\n[bold yellow]Message Statistics:[/bold yellow]")

    # Count messages by ID ranges
    msg_ids = sorted(merged_common.messages.keys())
    console.print(f"Message ID range: {min(msg_ids)} - {max(msg_ids)}")
    console.print(f"Total messages: {len(msg_ids)}")

    # Show first 5 messages
    table = Table(title="First 5 Messages")
    table.add_column("ID", style="cyan")
    table.add_column("Name", style="magenta")
    table.add_column("Fields", style="yellow")

    for msg_id in msg_ids[:5]:
        msg = merged_common.messages[msg_id]
        table.add_row(
            str(msg.id),
            msg.name,
            str(len(msg.fields))
        )

    console.print(table)

    # Test 4: Field type analysis
    console.print("\n[bold cyan]Test 4: Field Type Analysis[/bold cyan]")

    type_counts = {}
    array_fields = []
    enum_fields = []

    for msg in merged_common.messages.values():
        for field in msg.fields:
            # Count base types
            base_type = field.base_type
            type_counts[base_type] = type_counts.get(base_type, 0) + 1

            # Track arrays
            if field.is_array:
                array_fields.append((msg.name, field.name, field.type))

            # Track enum references
            if field.enum:
                enum_fields.append((msg.name, field.name, field.enum))

    # Show type distribution
    table = Table(title="Field Type Distribution")
    table.add_column("Type", style="cyan")
    table.add_column("Count", style="magenta")

    for field_type, count in sorted(type_counts.items(), key=lambda x: x[1], reverse=True)[:10]:
        table.add_row(field_type, str(count))

    console.print(table)

    console.print(f"\n[OK] Array fields: {len(array_fields)}")
    console.print(f"[OK] Enum-typed fields: {len(enum_fields)}")

    # Show some array examples
    if array_fields:
        console.print("\n[yellow]Array Field Examples:[/yellow]")
        for msg_name, field_name, field_type in array_fields[:5]:
            console.print(f"  • {msg_name}.{field_name}: {field_type}")

    console.print("\n[bold green][OK] All tests completed successfully![/bold green]")


if __name__ == "__main__":
    main()
