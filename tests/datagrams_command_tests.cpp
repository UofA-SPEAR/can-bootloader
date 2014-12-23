#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include <cstring>

#include <serializer/serialization.h>
#include "../command.h"

#define LEN(a) (sizeof(a) / sizeof(a[0]))

static void mock_command(int argc, cmp_ctx_t *arg_context, cmp_ctx_t *out_context, bootloader_config_t *config)
{
    mock().actualCall("command");
}

TEST_GROUP(ProtocolCommandTestGroup)
{
    serializer_t serializer;
    cmp_ctx_t command_builder;
    char command_data[30];

    void setup()
    {
        serializer_init(&serializer, command_data, sizeof command_data);
        serializer_cmp_ctx_factory(&command_builder, &serializer);
        memset(command_data, 0, sizeof command_data);
    }

    void teardown()
    {
        mock().clear();
    }
};

TEST(ProtocolCommandTestGroup, CommandIsCalled)
{
    command_t commands[1];
    commands[0].index = 0x00;
    commands[0].callback = mock_command;

    cmp_write_uint(&command_builder, 0x0);

    mock().expectOneCall("command");

    protocol_execute_command(command_data, sizeof command_data, commands, LEN(commands), NULL, 0, NULL);

    mock().checkExpectations();
}

TEST(ProtocolCommandTestGroup, CorrectCommandIsCalled)
{
    command_t commands[2];
    commands[0].index = 0x00;
    commands[0].callback = NULL;
    commands[1].index = 0x02;
    commands[1].callback = mock_command;

    // Write command index
    cmp_write_uint(&command_builder, 0x2);

    mock().expectOneCall("command");

    protocol_execute_command(command_data, sizeof command_data, commands, LEN(commands), NULL, 0, NULL);

    mock().checkExpectations();
}

void argc_log_command(int argc, cmp_ctx_t *dummy, cmp_ctx_t *out, bootloader_config_t *config)
{
    mock().actualCall("command").withIntParameter("argc", argc);
}

TEST(ProtocolCommandTestGroup, CorrectArgcIsSent)
{
    command_t commands[1];
    commands[0].index = 0x02;
    commands[0].callback = argc_log_command;

    // Write command index
    cmp_write_uint(&command_builder, 0x2);

    // Writes argument array length
    cmp_write_array(&command_builder, 42);

    mock().expectOneCall("command").withIntParameter("argc", 42);

    protocol_execute_command(command_data, sizeof command_data, commands, LEN(commands), NULL, 0, NULL);

    mock().checkExpectations();
}


void args_log_command(int argc, cmp_ctx_t *args, cmp_ctx_t *out, bootloader_config_t *config)
{
    int a, b;
    cmp_read_int(args, &a);
    cmp_read_int(args, &b);
    mock().actualCall("command")
          .withIntParameter("a", a)
          .withIntParameter("b", b);
}

TEST(ProtocolCommandTestGroup, CanReadArgs)
{
    command_t commands[1];
    commands[0].index = 0x02;
    commands[0].callback = args_log_command;

    // Command index
    cmp_write_uint(&command_builder, 0x2);

    // Argument array length
    cmp_write_array(&command_builder, 2);

    // Command arguments
    cmp_write_uint(&command_builder, 42);
    cmp_write_uint(&command_builder, 43);

    mock().expectOneCall("command")
          .withIntParameter("a", 42)
          .withIntParameter("b", 43);

    protocol_execute_command(command_data, sizeof command_data, commands, LEN(commands), NULL, 0, NULL);

    mock().checkExpectations();
}

void dummy_command(int argc, cmp_ctx_t *args, cmp_ctx_t *out, bootloader_config_t *config)
{
}

TEST(ProtocolCommandTestGroup, ExecuteReturnsZeroWhenValidCommand)
{
    int result;

    command_t commands[1];
    commands[0].index = 0x00;
    commands[0].callback = dummy_command;

    // Command index
    cmp_write_uint(&command_builder, 0x0);

    // Argument array length
    cmp_write_array(&command_builder, 0);

    result = protocol_execute_command(command_data, sizeof command_data, commands, LEN(commands), NULL, 0, NULL);
    CHECK_EQUAL(0, result);
}

TEST(ProtocolCommandTestGroup, ExecuteInvalidCommand)
{
    int result;
    command_t commands[1];
    commands[0].index = 0x00;
    commands[0].callback = dummy_command;

    // Invalid command index, here a float
    cmp_write_float(&command_builder, 3.14);

    // Argument array length
    cmp_write_array(&command_builder, 0);

    result = protocol_execute_command(command_data, sizeof command_data, commands, LEN(commands), NULL, 0, NULL);
    CHECK_EQUAL(-ERR_INVALID_COMMAND, result);
}

TEST(ProtocolCommandTestGroup, ExecuteWithoutArgumentsMeansArgcZero)
{
    int result;
    command_t commands[1];
    commands[0].index = 0x01;
    commands[0].callback = argc_log_command;

    cmp_write_uint(&command_builder, 1);

    mock().expectOneCall("command").withIntParameter("argc", 0);

    result = protocol_execute_command(command_data, sizeof command_data, commands, LEN(commands), NULL, 0, NULL);

    CHECK_EQUAL(0, result);

    mock().checkExpectations();
}

TEST(ProtocolCommandTestGroup, CallingNonExistingFunctionReturnsCorrectErrorCode)
{
    int result;

    command_t commands[1];
    commands[0].index = 0x00;
    commands[0].callback = argc_log_command;

    // Non existing command
    cmp_write_uint(&command_builder, 1);

    result = protocol_execute_command(command_data, sizeof command_data, commands, LEN(commands), NULL, 0, NULL);

    CHECK_EQUAL(-ERR_COMMAND_NOT_FOUND, result);
}

TEST_GROUP(ProtocolOutputCommand)
{
    serializer_t serializer;
    serializer_t out_serializer;
    cmp_ctx_t command_builder;
    cmp_ctx_t output_ctx;
    char command_data[30];
    char output_data[30];

    void setup()
    {
        serializer_init(&serializer, command_data, sizeof command_data);
        serializer_init(&out_serializer, output_data, sizeof output_data);
        serializer_cmp_ctx_factory(&command_builder, &serializer);
        serializer_cmp_ctx_factory(&output_ctx, &out_serializer);
        memset(command_data, 0, sizeof command_data);
        memset(output_data, 0, sizeof output_data);
    }

    void teardown()
    {
        mock().clear();
    }
};

void output_mock_command(int argc, cmp_ctx_t *args, cmp_ctx_t *out, bootloader_config_t *config)
{
    cmp_write_str(out, "Hello", 5);
}

TEST(ProtocolOutputCommand, CanPassOutputBuffer)
{
    int result;
    command_t commands[1];
    commands[0].index = 0x01;
    commands[0].callback = output_mock_command;

    cmp_write_uint(&command_builder, 1);

    protocol_execute_command(command_data, sizeof command_data, commands, LEN(commands), output_data, sizeof output_data, NULL);

    BYTES_EQUAL(0xa5, output_data[0]); // string of length 5
    STRCMP_EQUAL("Hello", &output_data[1]);
}

TEST(ProtocolOutputCommand, BytesCountIsReturned)
{
    int result;
    command_t commands[1];
    commands[0].index = 0x01;
    commands[0].callback = output_mock_command;

    cmp_write_uint(&command_builder, 1);

    result = protocol_execute_command(command_data, sizeof command_data, commands, LEN(commands), output_data, sizeof output_data, NULL);

    CHECK_EQUAL(6, result);
}

