#include <assert.h>
#include <stdio.h>

#define BOARD_MODE 1
#define BOARD_DEMO_UNIT_TEST
#include "../../src/main.c"

int main(void)
{
    BoardDemo_ButtonStateType button = {1U, 1U, 0U};
    uint8_t frame[3];
    uint8_t command = 0U;
    uint8_t sequence = 0U;
    unsigned i;

    assert(BoardDemo_ButtonPressed(&button, 0U, 0U) == 0U);
    assert(BoardDemo_ButtonPressed(&button, 1U, 4U) == 0U);
    assert(BoardDemo_ButtonPressed(&button, 0U, 5U) == 0U);
    assert(BoardDemo_ButtonPressed(&button, 0U, 24U) == 0U);
    assert(BoardDemo_ButtonPressed(&button, 0U, 25U) == 1U);
    assert(BoardDemo_ButtonPressed(&button, 0U, 200U) == 0U);
    assert(BoardDemo_ButtonPressed(&button, 1U, 201U) == 0U);
    assert(BoardDemo_ButtonPressed(&button, 1U, 221U) == 0U);
    assert(BoardDemo_ButtonPressed(&button, 0U, 222U) == 0U);
    assert(BoardDemo_ButtonPressed(&button, 0U, 242U) == 1U);

    for (i = 0U; i < 5U; i++)
    {
        BoardDemo_BuildNextFrame(frame, &command, &sequence);
        assert(frame[0] == 0xCAU);
        assert(frame[1] == (uint8_t)((i + 1U) & 3U));
        assert(frame[2] == (uint8_t)(i + 1U));
    }
    sequence = 255U;
    BoardDemo_BuildNextFrame(frame, &command, &sequence);
    assert(frame[2] == 0U);
    puts("PASS: TC-003 Tx debounce, LED cycle, magic and sequence.");
    return 0;
}
