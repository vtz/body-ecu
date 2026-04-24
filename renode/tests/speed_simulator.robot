*** Settings ***
Documentation     Speed simulator service smoke test in Renode.
...               Verifies the speed simulator SOME/IP service registers
...               its software-only fallback handlers during emulated boot.

Suite Setup       Setup
Suite Teardown    Teardown
Test Timeout      30 seconds

*** Keywords ***
Setup
    Execute Command           include @${CURDIR}/../body_ecu.resc

Teardown
    Execute Command           quit

*** Test Cases ***
Speed Service Initializes
    [Documentation]    Verify speed SOME/IP handlers are registered on boot.
    Start Emulation

    Wait For Line On Uart     Body ECU starting                    timeout=10
    Wait For Line On Uart     speed                                timeout=15
    Wait For Line On Uart     Body ECU ready                       timeout=10
