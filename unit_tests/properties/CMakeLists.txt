set(NAME unit_tests_properties)

set(SOURCES
  EPS/EPSDriverTest.cpp  
  imtq/imtqTest.cpp
  MissionPlan/MissionTestHelpers.cpp
  gyro/gyroTest.cpp
  Experiments/SunSDataPointTest.cpp
  Telecommands/SendFileTest.cpp
)

add_unit_tests(${NAME} ${SOURCES})

target_link_libraries(${NAME}    
    eps
    fm25w
    imtq
    mission
    mission_sail
    rapidcheck
    rapidcheck_gtest
    rapidcheck_gmock
    unit_tests_base
    gyro
    exp_suns
    obc_telecommands
)
