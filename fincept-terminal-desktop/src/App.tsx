import React, { useState } from 'react';
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Card, CardContent, CardDescription, CardFooter, CardHeader, CardTitle } from "@/components/ui/card";
import { Eye, EyeOff, TrendingUp, BarChart3, PieChart, LineChart } from "lucide-react";

interface GeometricPatternProps {
  children: React.ReactNode;
  gridSize?: number;
  lineColor?: string;
  backgroundColor?: string;
  opacity?: number;
}

const GeometricPattern: React.FC<GeometricPatternProps> = ({
  children,
  gridSize = 40,
  lineColor = '#333333',
  backgroundColor = '#0a0a0a',
  opacity = 0.4
}) => {
  return (
    <div
      className="fixed inset-0 w-screen h-screen overflow-hidden"
      style={{ backgroundColor }}
    >
      <svg
        className="absolute inset-0 w-full h-full"
        style={{ opacity }}
        preserveAspectRatio="xMidYMid slice"
      >
        <defs>
          <pattern
            id="geometric-grid"
            x="0"
            y="0"
            width={gridSize}
            height={gridSize}
            patternUnits="userSpaceOnUse"
          >
            <line
              x1="0"
              y1="0"
              x2={gridSize}
              y2={gridSize}
              stroke={lineColor}
              strokeWidth="1"
              opacity="0.8"
            />
            <line
              x1={gridSize}
              y1="0"
              x2="0"
              y2={gridSize}
              stroke={lineColor}
              strokeWidth="1"
              opacity="0.8"
            />
          </pattern>
        </defs>
        <rect width="100%" height="100%" fill="url(#geometric-grid)" />
      </svg>

      <div className="relative z-10 w-full h-full">
        {children}
      </div>
    </div>
  );
};

const LoginForm: React.FC = () => {
  const [showPassword, setShowPassword] = useState(false);
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [isLoading, setIsLoading] = useState(false);

  const handleLogin = async () => {
    setIsLoading(true);

    // Simulate login API call
    setTimeout(() => {
      setIsLoading(false);
      console.log('Login attempt:', { email, password });
    }, 2000);
  };

  return (
    <div className="w-full max-w-md">
      <Card className="bg-black/60 backdrop-blur-lg border-gray-700/50 shadow-2xl">
        <CardHeader className="space-y-1 pb-6">
          <div className="flex items-center justify-center mb-6">
            <div className="flex items-center space-x-3">
              <div className="p-2 bg-gradient-to-r from-blue-500 to-purple-600 rounded-lg">
                <TrendingUp className="h-8 w-8 text-white" />
              </div>
              <div>
                <h1 className="text-3xl font-bold text-white">Fincept</h1>
                <p className="text-xs text-gray-400 uppercase tracking-wider">Terminal</p>
              </div>
            </div>
          </div>
          <CardTitle className="text-2xl text-center text-white font-semibold">
            Welcome back
          </CardTitle>
          <CardDescription className="text-center text-gray-300 text-sm">
            Enter your credentials to access your financial terminal
          </CardDescription>
        </CardHeader>

        <CardContent className="space-y-6">
          <div className="space-y-2">
            <Label htmlFor="email" className="text-gray-200 text-sm font-medium">
              Email Address
            </Label>
            <Input
              id="email"
              type="email"
              placeholder="your@email.com"
              value={email}
              onChange={(e) => setEmail(e.target.value)}
              className="h-11 bg-gray-900/70 border-gray-600/50 text-white placeholder-gray-400 focus:border-blue-500 focus:ring-1 focus:ring-blue-500 transition-all"
            />
          </div>

          <div className="space-y-2">
            <Label htmlFor="password" className="text-gray-200 text-sm font-medium">
              Password
            </Label>
            <div className="relative">
              <Input
                id="password"
                type={showPassword ? "text" : "password"}
                placeholder="Enter your password"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                className="h-11 bg-gray-900/70 border-gray-600/50 text-white placeholder-gray-400 focus:border-blue-500 focus:ring-1 focus:ring-blue-500 pr-11 transition-all"
              />
              <Button
                type="button"
                onClick={() => setShowPassword(!showPassword)}
                className="absolute right-3 top-1/2 transform -translate-y-1/2 text-gray-400 hover:text-gray-200 bg-transparent hover:bg-transparent p-0 h-auto border-0"
              >
                {showPassword ? <EyeOff className="h-4 w-4" /> : <Eye className="h-4 w-4" />}
              </Button>
            </div>
          </div>

          <div className="flex items-center justify-between pt-2">
            <div className="flex items-center space-x-2">
              <input
                type="checkbox"
                id="remember"
                className="w-4 h-4 rounded border-gray-600 bg-gray-900/50 text-blue-600 focus:ring-blue-500 focus:ring-1"
              />
              <label htmlFor="remember" className="text-sm text-gray-300 cursor-pointer">
                Remember me
              </label>
            </div>
            <Button
              variant="link"
              className="text-blue-400 hover:text-blue-300 p-0 h-auto text-sm font-medium"
            >
              Forgot password?
            </Button>
          </div>
        </CardContent>

        <CardFooter className="flex flex-col space-y-4 pt-6">
          <Button
            onClick={handleLogin}
            className="w-full h-11 bg-gradient-to-r from-blue-600 to-purple-600 hover:from-blue-700 hover:to-purple-700 text-white font-medium transition-all duration-200 shadow-lg"
            disabled={isLoading}
          >
            {isLoading ? (
              <div className="flex items-center space-x-2">
                <div className="w-4 h-4 border-2 border-white/30 border-t-white rounded-full animate-spin"></div>
                <span>Signing in...</span>
              </div>
            ) : (
              "Sign in to Terminal"
            )}
          </Button>

          <div className="text-center text-sm text-gray-400">
            Don't have an account?{" "}
            <Button variant="link" className="text-blue-400 hover:text-blue-300 p-0 h-auto font-medium">
              Sign up for free
            </Button>
          </div>
        </CardFooter>
      </Card>
    </div>
  );
};

const FeatureCard: React.FC<{ icon: React.ReactNode; title: string; description: string }> = ({
  icon,
  title,
  description
}) => (
  <div className="p-6 bg-black/40 backdrop-blur-sm border border-gray-700/50 rounded-lg hover:border-gray-600/50 transition-colors">
    <div className="flex items-center space-x-3 mb-3">
      <div className="p-2 bg-gray-800/50 rounded-lg">
        {icon}
      </div>
      <h3 className="text-white font-semibold">{title}</h3>
    </div>
    <p className="text-gray-400 text-sm">{description}</p>
  </div>
);

const App: React.FC = () => {
  return (
    <GeometricPattern>
      <div className="min-h-screen w-full">
        {/* Desktop Layout */}
        <div className="hidden lg:flex">
          {/* Left Panel - Features */}
          <div className="flex-1 flex items-center justify-center p-12">
            <div className="max-w-lg">
              <div className="mb-12">
                <h2 className="text-4xl font-bold text-white mb-4">
                  Professional Financial Analysis
                </h2>
                <p className="text-gray-300 text-lg">
                  Advanced tools and real-time data for informed investment decisions
                </p>
              </div>

              <div className="space-y-6">
                <FeatureCard
                  icon={<BarChart3 className="h-5 w-5 text-blue-400" />}
                  title="Market Analysis"
                  description="Real-time market data with advanced charting and technical indicators"
                />
                <FeatureCard
                  icon={<PieChart className="h-5 w-5 text-green-400" />}
                  title="Portfolio Management"
                  description="Track and optimize your investment portfolio with detailed analytics"
                />
                <FeatureCard
                  icon={<LineChart className="h-5 w-5 text-purple-400" />}
                  title="Risk Assessment"
                  description="Comprehensive risk analysis tools to protect your investments"
                />
              </div>
            </div>
          </div>

          {/* Right Panel - Login */}
          <div className="flex-1 flex items-center justify-center p-12">
            <LoginForm />
          </div>
        </div>

        {/* Mobile/Tablet Layout */}
        <div className="lg:hidden flex flex-col min-h-screen">
          {/* Header */}
          <div className="flex-shrink-0 p-6 text-center">
            <div className="flex items-center justify-center space-x-3 mb-4">
              <div className="p-2 bg-gradient-to-r from-blue-500 to-purple-600 rounded-lg">
                <TrendingUp className="h-6 w-6 text-white" />
              </div>
              <div>
                <h1 className="text-2xl font-bold text-white">Fincept Terminal</h1>
              </div>
            </div>
            <p className="text-gray-300 text-sm">Professional Financial Analysis Platform</p>
          </div>

          {/* Content */}
          <div className="flex-1 flex items-center justify-center p-6">
            <div className="w-full max-w-sm">
              <LoginForm />
            </div>
          </div>

          {/* Features - Mobile */}
          <div className="flex-shrink-0 p-6 space-y-4">
            <div className="grid grid-cols-1 sm:grid-cols-3 gap-4">
              <div className="text-center p-4 bg-black/40 backdrop-blur-sm border border-gray-700/50 rounded-lg">
                <BarChart3 className="h-6 w-6 text-blue-400 mx-auto mb-2" />
                <h4 className="text-white font-medium text-sm">Market Data</h4>
              </div>
              <div className="text-center p-4 bg-black/40 backdrop-blur-sm border border-gray-700/50 rounded-lg">
                <PieChart className="h-6 w-6 text-green-400 mx-auto mb-2" />
                <h4 className="text-white font-medium text-sm">Portfolio</h4>
              </div>
              <div className="text-center p-4 bg-black/40 backdrop-blur-sm border border-gray-700/50 rounded-lg">
                <LineChart className="h-6 w-6 text-purple-400 mx-auto mb-2" />
                <h4 className="text-white font-medium text-sm">Analytics</h4>
              </div>
            </div>
          </div>
        </div>
      </div>
    </GeometricPattern>
  );
};

export default App;