# SPDX-License-Identifier: GPL-3.0-only
"""Small reference extractors; no application launch or input injection."""
import math

def wheel_angle(left, right, previous_deg=0, up=(0,1,0)):
    """Hands already in calibrated wheel space; clockwise is positive.

    The runtime transforms grip positions into that plane, retains last angle
    on lost tracking, and calls this only with deliberately grabbed hands.
    """
    delta=[b-a for a,b in zip(left,right)]
    x=delta[0]; y=sum(a*b for a,b in zip(delta,up))
    if math.hypot(x,y)<.025:return previous_deg
    angle=math.degrees(math.atan2(-y,x))
    return previous_deg+((angle-previous_deg+180)%360-180)

def axis_count(value, minimum=0, centre=128, maximum=255):
    value=max(-1,min(1,value))
    return round(centre+(maximum-centre)*value if value>=0 else centre+(centre-minimum)*value)

def steering_count(angle_deg,range_deg=270):
    if range_deg<=0:raise ValueError('Steering range must be positive')
    return axis_count(angle_deg/(range_deg/2))

def h_gate(x,z,previous=0,gate_x=.037,gate_z=.054,neutral_band=.012):
    """Snapped local knob travel in metres; centre bridge is neutral.

    z<0 is forward: left-forward=1, left-back=2, right-forward=3,
    right-back=4. A lateral crossing away from neutral keeps previous gear.
    """
    if abs(z)<neutral_band:return 0
    if abs(x)<gate_x*.55:return previous
    column=-1 if x<0 else 1
    if previous and (-1 if previous<=2 else 1)!=column:return previous
    if abs(z)<gate_z*.65:return previous
    return (1 if z<0 else 2)+(2 if column>0 else 0)

def pedal_count(trigger,deadzone=.04,gamma=1.4):
    if not 0<=deadzone<1 or gamma<=0:raise ValueError('Invalid pedal curve')
    value=max(0,min(1,(trigger-deadzone)/(1-deadzone)))**gamma
    return round(value*255)
