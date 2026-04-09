*** Settings ***
Documentation     Body ECU boot smoke test in Renode.
...               Verifies the firmware boots successfully and all systems
...               initialize on the emulated NUCLEO-H753ZI platform.

Suite Setup       Setup
Suite Teardown    Teardown
Test Timeout      30 seconds

*** Keywords ***
Setup
    Execute Command           include @${CURDIR}/../body_ecu.resc

Teardown
    Execute Command           quit

*** Test Cases ***
Body ECU Boots Successfully
    [Documentation]    Verify the Body ECU firmware boots and prints startup messages.
    Start Emulation

    Wait For Line On Uart     Body ECU starting        timeout=10
    Wait For Line On Uart     Platform:                timeout=5
    Wait For Line On Uart     Initializing systems     timeout=5
    Wait For Line On Uart     Body ECU ready           timeout=10

Body ECU Reports Board Name
    [Documentation]    Verify the platform name is printed during boot.
    Start Emulation

    Wait For Line On Uart     Body ECU starting        timeout=10
    Wait For Line On Uart     Platform: nucleo          timeout=10
