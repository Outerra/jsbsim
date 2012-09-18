#pragma once
#ifndef __JSBSIM_API_H__
#define __JSBSIM_API_H__

#ifdef JSBSIM_EXPORT
	#define JSBSIM_API __declspec(dllexport)
#else
	#define JSBSIM_API __declspec(dllimport)
#endif

#endif // __JSBSIM_API_H__
