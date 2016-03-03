<img src="https://splone.com/static/splone/images/splone.svg" width=200>

#splonebox

[Website](https://splone.com/splonebox) | 
[wiki] (https://github.com/splone/splonebox-core/wiki) |
[twitter] (https://twitter.com/sploneberlin/) 

[![Build Status](https://travis-ci.org/splone/splonebox-core.svg?branch=master)](https://travis-ci.org/splone/splonebox-core)
[![Coverage Status](https://coveralls.io/repos/github/splone/splonebox-core/badge.svg?branch=master)](https://coveralls.io/github/splone/splonebox-core?branch=master)

## What's behind it

splonebox is an open source network assessment tool with focus on modularity. It offers an ongoing analysis of a network and its devices. One major design decision features development of custom plugins.

## Motivation

Supervisory Control And Data Acquisition (SCADA) is the buzzword that summarizes devices of Industry Control Systems (ICS). These devices are highly specialized on controlling industry processes like production lines or even hole plants. Typically an ICS consists of Programmable Logic Controllers (PLCs), actuators and sensors. It is common, that industry networks also consist of additional devices like Human-Machine-Interfaces (HMI) or gateways

## Architecture

The splonebox consists of three components, which communicate through a central API. On the one hand, there is a core component that enforces security. On the other hand, we have several plugins, that do the actual work. Using this API the plugins are able to communicate with the splonebox core or even with other plugins.

<img src="https://raw.githubusercontent.com/wiki/splone/splonebox-core/images/network.png" width="400px">

## What's done so far

* see [Roadmap](https://github.com/splone/splonebox-core/wiki/Roadmap%20Progress)

## Contributing

* everyone is welcome
* see [developer wiki](https://github.com/splone/splonebox-core/wiki#for-developers) for details

## License 

* the splonebox is licensed under AGPLv3. 
* see [License](https://github.com/splone/splonebox-core/blob/master/LICENSE) for details
